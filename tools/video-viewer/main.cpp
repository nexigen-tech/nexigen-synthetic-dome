/* Minimal Qt6/OpenGL video viewer for the skeleton.
 *
 * Loads the VirtualVideo component, runs it on a background RX thread, and
 * renders the latest RGBA8888 frame in a QOpenGLWidget.  This proves the
 * RX/GUI thread separation and the OpenGL display path on a configured host.
 *
 * Headless latency measurement mode:
 *   nxd_video_viewer <descriptor> <plugin> --headless --frames N
 * runs until N distinct frames have been received on the GUI thread, prints
 * RX-to-display latency statistics to stderr, and exits.  This mode is used by
 * automated tests and does not require a display server or OpenGL.
 */

#include "nxd/contracts/VideoAggregateErrors.hpp"
#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/contracts/VideoHeartbeat.hpp"
#include "nxd/host/harness.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLWidget>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>

namespace host = nxd::host;
using nxd::contracts::VideoFrame;

namespace {

constexpr std::uint32_t kWidth = 1920;
constexpr std::uint32_t kHeight = 1080;
constexpr std::size_t kPayloadSize = static_cast<std::size_t>(kWidth) * kHeight * 4u;

struct LatencyStats {
    std::size_t count = 0;
    std::uint64_t min_us = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t max_us = 0;
    std::uint64_t sum_us = 0;

    void add(std::uint64_t us) noexcept {
        ++count;
        min_us = std::min(min_us, us);
        max_us = std::max(max_us, us);
        sum_us += us;
    }

    [[nodiscard]] std::uint64_t mean_us() const noexcept {
        return count > 0 ? sum_us / static_cast<std::uint64_t>(count) : 0;
    }
};

struct SharedFrame {
    QMutex mutex;
    std::vector<std::uint8_t> pixels;
    std::uint32_t transport_sequence = 0;
    std::uint32_t rx_data_frames = 0;
    std::chrono::steady_clock::time_point rx_time;
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
            std::cerr << "viewer: load failed: " << static_cast<int>(st.error()) << "\n";
            return;
        }
        if (const auto st = harness.initialize(R"({"source_name":"Camera_Front_Left"})"); !st) {
            std::cerr << "viewer: initialize failed: " << static_cast<int>(st.error()) << "\n";
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
            std::cerr << "viewer: arm failed: " << static_cast<int>(st.error()) << "\n";
            return;
        }

        std::uint32_t rx_data_frames = 0;
        while (!stop_flag_->load()) {
            // Run one base frame (1 ms) at a time so the GUI thread can update.
            // Simulation time advances across calls because the harness tracks
            // the total number of frames run.
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
                shared_->rx_time = std::chrono::steady_clock::now();
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

class HeadlessDisplay : public QObject {
    Q_OBJECT

  public:
    HeadlessDisplay(SharedFrame* shared, std::size_t target_frames, QObject* parent = nullptr)
        : QObject(parent), shared_(shared), target_frames_(target_frames) {}

    [[nodiscard]] const LatencyStats& latency_stats() const noexcept { return latency_stats_; }

  signals:
    void target_reached();

  public slots:
    void tick() {
        std::uint32_t sequence = 0;
        std::chrono::steady_clock::time_point rx_time;
        bool has_frame = false;
        {
            QMutexLocker lock(&shared_->mutex);
            if (shared_->has_frame) {
                sequence = shared_->transport_sequence;
                rx_time = shared_->rx_time;
                has_frame = true;
            }
        }

        if (!has_frame) {
            return;
        }

        // Measure RX-to-display latency only for newly observed frames.
        if (sequence != last_displayed_sequence_) {
            const auto now = std::chrono::steady_clock::now();
            const auto us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(now - rx_time).count());
            latency_stats_.add(us);
            last_displayed_sequence_ = sequence;
            if (target_frames_ > 0 && latency_stats_.count >= target_frames_) {
                emit target_reached();
            }
        }
    }

  private:
    SharedFrame* shared_;
    std::size_t target_frames_ = 0;
    LatencyStats latency_stats_;
    std::uint32_t last_displayed_sequence_ = std::numeric_limits<std::uint32_t>::max();
};

class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    VideoWidget(SharedFrame* shared, std::size_t target_frames, QWidget* parent = nullptr)
        : QOpenGLWidget(parent), shared_(shared), target_frames_(target_frames) {}

    [[nodiscard]] const LatencyStats& latency_stats() const noexcept { return latency_stats_; }

  signals:
    void target_reached();

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
        std::uint32_t sequence = 0;
        std::chrono::steady_clock::time_point rx_time;
        {
            QMutexLocker lock(&shared_->mutex);
            if (shared_->has_frame) {
                pixels = shared_->pixels;
                sequence = shared_->transport_sequence;
                rx_time = shared_->rx_time;
                has_frame = true;
            }
        }

        if (!has_frame || pixels.size() != kPayloadSize) {
            return;
        }

        // Measure RX-to-display latency only for newly displayed frames.
        if (sequence != last_displayed_sequence_) {
            const auto now = std::chrono::steady_clock::now();
            const auto us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(now - rx_time).count());
            latency_stats_.add(us);
            last_displayed_sequence_ = sequence;
            if (target_frames_ > 0 && latency_stats_.count >= target_frames_) {
                emit target_reached();
            }
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
    std::size_t target_frames_ = 0;
    std::unique_ptr<QOpenGLTexture> texture_;
    std::uint32_t last_displayed_sequence_ = std::numeric_limits<std::uint32_t>::max();
    LatencyStats latency_stats_;
};

[[nodiscard]] bool parse_headless_arg(int argc, char** argv) noexcept {
    for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--headless") {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t parse_frames_arg(int argc, char** argv) noexcept {
    for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames" && i + 1 < argc) {
            std::size_t value = 0;
            const char* str = argv[i + 1];
            while (*str >= '0' && *str <= '9') {
                value = value * 10 + static_cast<std::size_t>(*str - '0');
                ++str;
            }
            if (*str == '\0' && value > 0) {
                return value;
            }
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <virtual_video.descriptor.json> <libnxd_example_virtual_video_cpp.so>"
                     " [--headless] [--frames N]\n";
        return 1;
    }

    const bool headless = parse_headless_arg(argc, argv);
    const std::size_t target_frames = parse_frames_arg(argc, argv);

    std::unique_ptr<QCoreApplication> app;
    if (headless) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        app = std::make_unique<QApplication>(argc, argv);
    }

    SharedFrame shared;
    std::atomic<bool> stop_flag{false};

    RxThread rx_thread(argv[1], argv[2], &shared, &stop_flag);
    rx_thread.start();

    std::optional<HeadlessDisplay> display;
    std::optional<VideoWidget> widget;
    QTimer timer;
    if (headless) {
        display.emplace(&shared, target_frames);
        QObject::connect(&timer, &QTimer::timeout, &*display, &HeadlessDisplay::tick);
        if (target_frames > 0) {
            QObject::connect(&*display, &HeadlessDisplay::target_reached, app.get(),
                             &QCoreApplication::quit);
        }
        timer.start(20); // ~50 FPS display refresh
    } else {
        widget.emplace(&shared, target_frames);
        widget->setWindowTitle("NexDome Video Skeleton Viewer");
        widget->resize(960, 540);
        widget->show();

        QObject::connect(&timer, &QTimer::timeout, &*widget, QOverload<>::of(&VideoWidget::update));
        if (target_frames > 0) {
            QObject::connect(&*widget, &VideoWidget::target_reached, app.get(),
                             &QCoreApplication::quit);
        }
        timer.start(20); // ~50 FPS display refresh
    }

    const int rc = app->exec();
    stop_flag.store(true);
    rx_thread.wait(1000);

    if (target_frames > 0) {
        const LatencyStats* stats = nullptr;
        if (headless) {
            stats = &display->latency_stats();
        } else {
            stats = &widget->latency_stats();
        }
        if (stats != nullptr && stats->count > 0) {
            std::cerr << "latency: count=" << stats->count << " min_us=" << stats->min_us
                      << " max_us=" << stats->max_us << " mean_us=" << stats->mean_us() << "\n";
        }
    }

    return rc;
}

#include "main.moc"
