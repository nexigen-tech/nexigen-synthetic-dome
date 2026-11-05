/* Headless test for the video receiver plugin.
 *
 * Uses manual stepping so that input frames can be injected per frame and
 * the emitted aggregate can be inspected.
 */

#include "nxd/contracts/VideoAggregateErrors.hpp"
#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/host/component.hpp"
#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace host = nxd::host;
using nxd::contracts::Codec;
using nxd::contracts::VideoAggregateErrors;
using nxd::contracts::VideoAggregateStatus;
using nxd::contracts::VideoFrame;

namespace {

void fill_frame(VideoFrame& frame, std::uint32_t transport_sequence, std::uint32_t content_sequence,
                std::uint32_t flags = 0) {
    std::memset(&frame, 0, sizeof(VideoFrame));
    frame.stream_id = 1;
    frame.transport_sequence = transport_sequence;
    frame.content_sequence = content_sequence;
    frame.width = 1920;
    frame.height = 1080;
    frame.codec = Codec::Raw;
    frame.payload_length = 1920u * 1080u * 4u;
    frame.flags = flags;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <descriptor.json> <video-opengl-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* plugin_path = argv[2];

    host::HeadlessHarness harness(plugin_path, descriptor_path);
    if (const auto st = harness.load(); !st) {
        std::cerr << "load failed\n";
        return 2;
    }
    if (const auto st = harness.initialize(R"({"source_name":"Camera_Front_Left"})"); !st) {
        std::cerr << "initialize failed\n";
        return 3;
    }

    std::vector<host::PortBinding> bindings;
    bindings.push_back(host::PortBinding{
        .name = "in",
        .direction = host::PortDescriptor::Direction::In,
        .element_size = sizeof(VideoFrame),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    bindings.push_back(host::PortBinding{
        .name = "aggregate",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(VideoAggregateErrors),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    harness.configure_ports(std::move(bindings));

    if (const auto st = harness.arm(42); !st) {
        std::cerr << "arm failed\n";
        return 4;
    }

    auto* comp = harness.component();
    if (comp == nullptr) {
        std::cerr << "component missing\n";
        return 5;
    }

    auto in_buf = harness.port_buffer<VideoFrame>("in");
    auto out_buf = harness.port_buffer<VideoAggregateErrors>("aggregate");
    if (in_buf.size() != 1 || out_buf.size() != 1) {
        std::cerr << "buffer size mismatch\n";
        return 6;
    }

    // Step 1: feed frame 0.
    fill_frame(in_buf[0], 0, 0);
    {
        NxdFrameContext ctx{};
        ctx.frame_no = 0;
        ctx.sim_time_ns = 0;
        ctx.hw_time_ns = 0;
        if (const auto st = comp->step(&ctx); !st) {
            std::cerr << "step 0 failed\n";
            return 10;
        }
        if (out_buf[0].status != VideoAggregateStatus::Valid) {
            std::cerr << "step 0: expected Valid aggregate\n";
            return 11;
        }
    }

    // Step 2: skip transport sequence 1 (inject gap), feed frame 2.
    fill_frame(in_buf[0], 2, 1);
    {
        NxdFrameContext ctx{};
        ctx.frame_no = 1;
        ctx.sim_time_ns = 1'000'000;
        ctx.hw_time_ns = 1'000'000;
        if (const auto st = comp->step(&ctx); !st) {
            std::cerr << "step 1 failed\n";
            return 20;
        }
        if (out_buf[0].flags == 0) {
            std::cerr << "step 1: expected gap flag\n";
            return 21;
        }
        if (out_buf[0].status != VideoAggregateStatus::MissingLines) {
            std::cerr << "step 1: expected MissingLines aggregate\n";
            return 22;
        }
    }

    // Step 3: feed frame with error flags.
    fill_frame(in_buf[0], 3, 2, 1);
    {
        NxdFrameContext ctx{};
        ctx.frame_no = 2;
        ctx.sim_time_ns = 2'000'000;
        ctx.hw_time_ns = 2'000'000;
        if (const auto st = comp->step(&ctx); !st) {
            std::cerr << "step 2 failed\n";
            return 30;
        }
        if ((out_buf[0].flags & 4u) == 0) {
            std::cerr << "step 2: expected error flag\n";
            return 31;
        }
        if (out_buf[0].status != VideoAggregateStatus::CrcError) {
            std::cerr << "step 2: expected CrcError aggregate\n";
            return 32;
        }
    }

    comp->stop();
    return 0;
}
