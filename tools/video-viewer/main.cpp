/* Minimal Qt6/OpenGL video viewer for the skeleton.
 *
 * Loads the VirtualVideo component, runs it on a background RX thread, and
 * renders the latest RGBA8888 frame in a QOpenGLWidget.  This proves the
 * RX/GUI thread separation and the OpenGL display path on a configured host.
 */

#include "nxd/contracts/VideoAggregateErrors.hpp"
#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/contracts/VideoHeartbeat.hpp"
#include "nxd/host/harness.hpp"

#include <QApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLWidget>
#include <QPainter>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

namespace host = nxd::host;
using nxd::contracts::VideoFrame;

namespace {

constexpr std::uint32_t kWidth = 1920;
constexpr std::uint32_t kHeight = 1080;
constexpr std::size_t kPayloadSize = static_cast<std::size_t>(kWidth) * kHeight * 4u;

struct SharedFrame {
    QMutex mutex;
    std::vector<std::uint8_t> pixels;
    std::uint32_t transport_sequence = 0;
    std::uint32_t rx_data_frames = 0;
    bool has_frame = false;
};

class RxThread : public QThread {
  public:
    RxThread(const QString& descriptor_path, const QString& plugin_path, SharedFrame* shared,
             std::atomic<bool>* stop_flag, QObject* parent = nullptr)
        : QThread(parent),
          descriptor_path_(descriptor_path),
          plugin_path_(plugin_path),
          shared_(shared),
          stop_flag_(stop_flag) {}

  protected:
    void run() override {
        host::HeadlessHarness harness(plugin_path_.toStdString(), descriptor_path_.toStdString());
        if (const auto st = harness.load(); !st) {
            std::cerr << "viewer: load failed\n";
            return;
        }
        if (const auto st = harness.initialize(R"({"source_name":"Camera_Front_Left"})"); !st) {
            std::cerr << "viewer: initialize failed\n";
            return;
        }

        std::vector<host::PortBinding> bindings;
        bindings.push_back(host::PortBinding{
            .name = "out",
            .direction = host::PortDescriptor::Direction::Out,
            .element_size = sizeof(VideoFrame),
            .count = 1,
            .layout_hash = std::nullopt,
            .buffer = {},
        });
        bindings.push_back(host::PortBinding{
            .name = "heartbeat",
            .direction = host::PortDescriptor::Direction::Out,
            .element_size = sizeof(nxd::contracts::VideoHeartbeat),
            .count = 1,
            .layout_hash = std::nullopt,
            .buffer = {},
        });
        bindings.push_back(host::PortBinding{
            .name = "aggregate",
            .direction = host::PortDescriptor::Direction::Out,
            .element_size = sizeof(nxd::contracts::VideoAggregateErrors),
            .count = 1,
            .layout_hash = std::nullopt,
            .buffer = {},
        });
        harness.configure_ports(std::move(bindings));

        if (const auto st = harness.arm(42); !st) {
            std::cerr << "viewer: arm failed\n";
            return;
        }

        std::uint32_t rx_data_frames = 0;
        while (!stop_flag_->load()) {
            // Run one base frame (1 ms) at a time so the GUI thread can update.
            if (const auto st = harness.run(1); !st) {
                std::cerr << "viewer: run failed\n";
                return;
            }
            auto out_buf = harness.port_buffer<VideoFrame>("out");
            if (out_buf.size() == 1 && out_buf[0].payload_length == kPayloadSize) {
                QMutexLocker lock(&shared_->mutex);
                shared_->pixels.resize(kPayloadSize);
                std::memcpy(shared_->pixels.data(), out_buf[0].payload.data(), kPayloadSize);
                shared_->transport_sequence = out_buf[0].transport_sequence;
                shared_->rx_data_frames = ++rx_data_frames;
                shared_->has_frame = true;
            }
        }
    }

  private:
    QString descriptor_path_;
    QString plugin_path_;
    SharedFrame* shared_;
    std::atomic<bool>* stop_flag_;
};

class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  public:
    explicit VideoWidget(SharedFrame* shared, QWidget* parent = nullptr)
        : QOpenGLWidget(parent), shared_(shared) {}

  protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        texture_ = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        texture_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    }

    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT);

        std::vector<std::uint8_t> pixels;
        bool has_frame = false;
        {
            QMutexLocker lock(&shared_->mutex);
            if (shared_->has_frame) {
                pixels = shared_->pixels;
                has_frame = true;
            }
        }

        if (!has_frame || pixels.size() != kPayloadSize) {
            return;
        }

        if (texture_->width() != static_cast<int>(kWidth) ||
            texture_->height() != static_cast<int>(kHeight)) {
            texture_->destroy();
            texture_->setSize(static_cast<int>(kWidth), static_cast<int>(kHeight));
            texture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
            texture_->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        }
        texture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, pixels.data());

        texture_->bind();
        glEnable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, 1.0f);
        glEnd();

        texture_->release();
    }

    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
    }

  private:
    SharedFrame* shared_;
    std::unique_ptr<QOpenGLTexture> texture_;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <virtual_video.descriptor.json> <libnxd_example_virtual_video_cpp.so>\n";
        return 1;
    }

    QApplication app(argc, argv);

    SharedFrame shared;
    std::atomic<bool> stop_flag{false};

    RxThread rx_thread(argv[1], argv[2], &shared, &stop_flag);
    rx_thread.start();

    VideoWidget widget(&shared);
    widget.setWindowTitle("NexDome Video Skeleton Viewer");
    widget.resize(960, 540);
    widget.show();

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &widget, QOverload<>::of(&VideoWidget::update));
    timer.start(20); // ~50 FPS display refresh

    const int rc = app.exec();
    stop_flag.store(true);
    rx_thread.wait(1000);
    return rc;
}
