/* Headless end-to-end test for the deterministic virtual CAN source.
 *
 * Each scenario uses a fresh harness because the headless driver resets
 * frame_no/sim_time at the start of every run.  Running N frames therefore
 * leaves the N-th frame (frame_no == N-1) in the output buffer.
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace host = nxd::host;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;

namespace {

[[nodiscard]] RawCanBatchClassical run_to_frame(const char* descriptor_path,
                                                 const char* plugin_path,
                                                 std::uint64_t frame_count) {
    host::HeadlessHarness harness(plugin_path, descriptor_path);
    if (const auto st = harness.load(); !st) {
        std::cerr << "load failed\n";
        std::exit(2);
    }
    if (const auto st = harness.initialize(R"({"source_name":"CAN_A"})"); !st) {
        std::cerr << "initialize failed\n";
        std::exit(3);
    }

    std::vector<host::PortBinding> bindings;
    bindings.push_back(host::PortBinding{
        .name = "out",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(RawCanBatchClassical),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    harness.configure_ports(std::move(bindings));

    if (const auto st = harness.arm(42); !st) {
        std::cerr << "arm failed\n";
        std::exit(4);
    }

    if (const auto st = harness.run(frame_count); !st) {
        std::cerr << "run(" << frame_count << ") failed\n";
        std::exit(5);
    }

    auto out_buf = harness.port_buffer<RawCanBatchClassical>("out");
    if (out_buf.size() != 1) {
        std::cerr << "buffer size mismatch\n";
        std::exit(6);
    }
    return out_buf[0];
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <descriptor.json> <virtual-can-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* plugin_path = argv[2];

    // Frame 0: one scheduled data frame on 0x100.
    {
        const auto batch = run_to_frame(descriptor_path, plugin_path, 1);
        if (batch.count != 1) {
            std::cerr << "frame 0: expected count 1, got " << batch.count << "\n";
            return 10;
        }
        if (batch.assigned_frame != 0) {
            std::cerr << "frame 0: expected assigned_frame 0, got " << batch.assigned_frame << "\n";
            return 11;
        }
        const auto& f = batch.frames[0];
        if (f.can_id != 0x100 || f.direction != Direction::SelfRx || f.kind != Kind::Data ||
            f.payload_length != 4 || f.payload[0] != 0x01 || f.payload[3] != 0x04) {
            std::cerr << "frame 0: unexpected frame content\n";
            return 12;
        }
    }

    // Frame 1: empty watermark batch.
    {
        const auto batch = run_to_frame(descriptor_path, plugin_path, 2);
        if (batch.count != 0) {
            std::cerr << "frame 1: expected empty watermark, got count " << batch.count << "\n";
            return 15;
        }
    }

    // Frame 2: three frames sorted by CAN id: 0x080, 0x100, 0x701.
    {
        const auto batch = run_to_frame(descriptor_path, plugin_path, 3);
        if (batch.count != 3) {
            std::cerr << "frame 2: expected count 3, got " << batch.count << "\n";
            return 20;
        }
        if (batch.frames[0].can_id != 0x080) {
            std::cerr << "frame 2: first frame should be 0x080\n";
            return 21;
        }
        if (batch.frames[1].can_id != 0x100) {
            std::cerr << "frame 2: second frame should be 0x100\n";
            return 22;
        }
        if (batch.frames[2].can_id != 0x701 || batch.frames[2].kind != Kind::Heartbeat) {
            std::cerr << "frame 2: third frame should be heartbeat 0x701\n";
            return 23;
        }
        for (std::uint32_t i = 0; i < batch.count; ++i) {
            if (batch.frames[i].direction != Direction::SelfRx) {
                std::cerr << "frame 2: frame " << i << " is not SelfRx\n";
                return 24;
            }
        }
    }

    // Frame 4: one error frame on 0x200.
    {
        const auto batch = run_to_frame(descriptor_path, plugin_path, 5);
        if (batch.count != 1 || batch.frames[0].can_id != 0x200 ||
            batch.frames[0].kind != Kind::Error) {
            std::cerr << "frame 4: expected one error frame on 0x200\n";
            return 30;
        }
    }

    // Frame 5: empty watermark batch.
    {
        const auto batch = run_to_frame(descriptor_path, plugin_path, 6);
        if (batch.count != 0) {
            std::cerr << "frame 5: expected empty watermark, got count " << batch.count << "\n";
            return 40;
        }
    }

    // Deterministic replay: a fresh six-frame run ends with the same batch.
    {
        const auto original = run_to_frame(descriptor_path, plugin_path, 6);
        const auto replayed = run_to_frame(descriptor_path, plugin_path, 6);
        if (std::memcmp(&original, &replayed, sizeof(RawCanBatchClassical)) != 0) {
            std::cerr << "replay mismatch\n";
            return 54;
        }
    }

    return 0;
}
