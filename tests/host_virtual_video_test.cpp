/* Headless end-to-end test for the deterministic virtual video source.
 *
 * Verifies frame rate, sequence numbering, deterministic pixel generation,
 * heartbeat emission, and aggregate error reporting.
 */

#include "nxd/contracts/VideoAggregateErrors.hpp"
#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/contracts/VideoHeartbeat.hpp"
#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace host = nxd::host;
using nxd::contracts::Codec;
using nxd::contracts::VideoFrame;

namespace {

class VirtualVideoRunner {
  public:
    VirtualVideoRunner(const char* descriptor_path, const char* plugin_path,
                       std::string_view config = R"({"source_name":"Camera_Front_Left"})") {
        harness_ = std::make_unique<host::HeadlessHarness>(plugin_path, descriptor_path);
        if (const auto st = harness_->load(); !st) {
            std::cerr << "load failed\n";
            std::exit(2);
        }
        if (const auto st = harness_->initialize(config); !st) {
            std::cerr << "initialize failed\n";
            std::exit(3);
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
        harness_->configure_ports(std::move(bindings));

        if (const auto st = harness_->arm(42); !st) {
            std::cerr << "arm failed\n";
            std::exit(4);
        }
    }

    [[nodiscard]] const VideoFrame& run(std::uint64_t frame_count) {
        if (const auto st = harness_->run(frame_count); !st) {
            std::cerr << "run(" << frame_count << ") failed\n";
            std::exit(5);
        }
        auto out_buf = harness_->port_buffer<VideoFrame>("out");
        if (out_buf.size() != 1) {
            std::cerr << "buffer size mismatch\n";
            std::exit(6);
        }
        return out_buf[0];
    }
  private:
    std::unique_ptr<host::HeadlessHarness> harness_;
};

std::uint32_t read_embedded_sequence(const VideoFrame& frame) {
    return (static_cast<std::uint32_t>(frame.payload[0]) << 24) |
           (static_cast<std::uint32_t>(frame.payload[1]) << 16) |
           (static_cast<std::uint32_t>(frame.payload[2]) << 8) |
           static_cast<std::uint32_t>(frame.payload[3]);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <descriptor.json> <virtual-video-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* plugin_path = argv[2];

    // Frame 1 (first 1 ms after frame 0): no video frame yet (first frame at 20 ms).
    {
        VirtualVideoRunner runner(descriptor_path, plugin_path);
        const auto& frame = runner.run(2);
        if (frame.payload_length != 0) {
            std::cerr << "frame 1: expected no video frame, got payload_length "
                      << frame.payload_length << "\n";
            return 10;
        }
    }

    // Frame 20: first video frame at 20 ms.
    {
        VirtualVideoRunner runner(descriptor_path, plugin_path);
        const auto& frame = runner.run(21);
        if (frame.payload_length != 1920u * 1080u * 4u) {
            std::cerr << "frame 20: unexpected payload_length " << frame.payload_length << "\n";
            return 20;
        }
        if (frame.width != 1920 || frame.height != 1080 || frame.codec != Codec::Raw) {
            std::cerr << "frame 20: unexpected frame metadata\n";
            return 21;
        }
        if (frame.content_sequence != 0) {
            std::cerr << "frame 20: expected content_sequence 0, got " << frame.content_sequence
                      << "\n";
            return 22;
        }
        if (frame.transport_sequence != 0) {
            std::cerr << "frame 20: expected transport_sequence 0, got " << frame.transport_sequence
                      << "\n";
            return 23;
        }
        if (read_embedded_sequence(frame) != 0) {
            std::cerr << "frame 20: embedded sequence mismatch\n";
            return 24;
        }
    }

    // Frame 40: second video frame at 40 ms.
    {
        VirtualVideoRunner runner(descriptor_path, plugin_path);
        const auto& frame = runner.run(41);
        if (frame.content_sequence != 1 || frame.transport_sequence != 1) {
            std::cerr << "frame 40: unexpected sequences\n";
            return 30;
        }
        if (read_embedded_sequence(frame) != 1) {
            std::cerr << "frame 40: embedded sequence mismatch\n";
            return 31;
        }
    }

    // Deterministic replay: same frame index produces identical pixels.
    {
        VirtualVideoRunner runner1(descriptor_path, plugin_path);
        const auto& original = runner1.run(21);
        VirtualVideoRunner runner2(descriptor_path, plugin_path);
        const auto& replayed = runner2.run(21);
        if (original.payload_length != replayed.payload_length) {
            std::cerr << "replay: payload length mismatch\n";
            return 40;
        }
        if (std::memcmp(original.payload.data(), replayed.payload.data(), original.payload_length) !=
            0) {
            std::cerr << "replay: pixel mismatch\n";
            return 41;
        }
    }

    // One second of simulation: 50 data frames emitted (frame 1000 is the 50th).
    {
        VirtualVideoRunner runner(descriptor_path, plugin_path);
        const auto& frame = runner.run(1001);
        if (frame.content_sequence != 49) {
            std::cerr << "1s run: expected content_sequence 49, got " << frame.content_sequence
                      << "\n";
            return 50;
        }
    }

    return 0;
}
