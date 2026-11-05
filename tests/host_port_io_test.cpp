/* Headless typed port I/O end-to-end test. */

#include "nxd/host/harness.hpp"
#include "sample.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace host = nxd::host;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0]
                  << " <descriptor.json> <descriptor.hash> <port-io-plugin.so>\n";
        return 1;
    }

    host::HeadlessHarness harness(argv[3], argv[1]);
    if (const auto st = harness.load(); !st) {
        std::cerr << "load failed\n";
        return 2;
    }
    if (const auto st = harness.initialize(); !st) {
        std::cerr << "initialize failed\n";
        return 3;
    }

    std::array<std::uint8_t, 32> layout_hash{};
    std::memset(layout_hash.data(), 0x11, layout_hash.size());

    std::vector<host::PortBinding> bindings;
    bindings.push_back(host::PortBinding{
        .name = "in",
        .direction = host::PortDescriptor::Direction::In,
        .element_size = sizeof(nxd_test::PortIoSample),
        .count = 1,
        .layout_hash = layout_hash,
        .buffer = {},
    });
    bindings.push_back(host::PortBinding{
        .name = "out",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(nxd_test::PortIoSample),
        .count = 1,
        .layout_hash = layout_hash,
        .buffer = {},
    });
    harness.configure_ports(std::move(bindings));

    if (const auto st = harness.arm(12345); !st) {
        std::cerr << "arm failed\n";
        return 4;
    }

    auto in_buf = harness.port_buffer<nxd_test::PortIoSample>("in");
    auto out_buf = harness.port_buffer<nxd_test::PortIoSample>("out");
    if (in_buf.size() != 1 || out_buf.size() != 1) {
        std::cerr << "buffer size mismatch\n";
        return 5;
    }

    in_buf[0] = nxd_test::PortIoSample{.sequence = 7, .value = 42};
    if (const auto st = harness.run(1); !st) {
        std::cerr << "run failed\n";
        return 6;
    }

    if (out_buf[0].sequence != 7 || out_buf[0].value != 43) {
        std::cerr << "unexpected output: sequence=" << out_buf[0].sequence
                  << " value=" << out_buf[0].value << "\n";
        return 7;
    }

    // Deterministic replay: same input produces same output.
    in_buf[0] = nxd_test::PortIoSample{.sequence = 7, .value = 42};
    if (const auto st = harness.run(1); !st) {
        std::cerr << "second run failed\n";
        return 8;
    }
    if (out_buf[0].sequence != 7 || out_buf[0].value != 43) {
        std::cerr << "replay mismatch\n";
        return 9;
    }

    // Layout-hash mismatch must be rejected during arming.
    {
        host::HeadlessHarness bad_harness(argv[3], argv[1]);
        if (const auto st = bad_harness.load(); !st) {
            std::cerr << "bad harness load failed\n";
            return 10;
        }
        if (const auto st = bad_harness.initialize(); !st) {
            std::cerr << "bad harness initialize failed\n";
            return 11;
        }

        std::array<std::uint8_t, 32> bad_hash{}; // all zeros, does not match descriptor
        std::vector<host::PortBinding> bad_bindings;
        bad_bindings.push_back(host::PortBinding{
            .name = "in",
            .direction = host::PortDescriptor::Direction::In,
            .element_size = sizeof(nxd_test::PortIoSample),
            .count = 1,
            .layout_hash = bad_hash,
            .buffer = {},
        });
        bad_bindings.push_back(host::PortBinding{
            .name = "out",
            .direction = host::PortDescriptor::Direction::Out,
            .element_size = sizeof(nxd_test::PortIoSample),
            .count = 1,
            .layout_hash = bad_hash,
            .buffer = {},
        });
        bad_harness.configure_ports(std::move(bad_bindings));

        const auto st = bad_harness.arm(12345);
        if (st) {
            std::cerr << "arm with bad hash should have failed\n";
            return 12;
        }
        if (bad_harness.component()->last_status() != NXD_ERROR_ABI_MISMATCH) {
            std::cerr << "expected NXD_ERROR_ABI_MISMATCH\n";
            return 13;
        }
    }

    std::cout << "ok\n";
    return 0;
}
