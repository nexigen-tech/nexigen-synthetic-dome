/* Cross-language golden test for typed port I/O.
 *
 * Loads the C++ and Rust PortIoTest plugins with the same descriptor,
 * feeds identical input samples, and verifies both produce the same output.
 */

#include "nxd/host/harness.hpp"
#include "sample.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace host = nxd::host;

static std::array<std::uint8_t, 32> layout_hash() noexcept {
    std::array<std::uint8_t, 32> h{};
    std::memset(h.data(), 0x11, h.size());
    return h;
}

static std::vector<host::PortBinding> make_bindings() {
    const auto hash = layout_hash();
    std::vector<host::PortBinding> bindings;
    bindings.push_back(host::PortBinding{
        .name = "in",
        .direction = host::PortDescriptor::Direction::In,
        .element_size = sizeof(nxd_test::PortIoSample),
        .count = 1,
        .layout_hash = hash,
        .buffer = {},
    });
    bindings.push_back(host::PortBinding{
        .name = "out",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(nxd_test::PortIoSample),
        .count = 1,
        .layout_hash = hash,
        .buffer = {},
    });
    return bindings;
}

static bool run_plugin(const char* descriptor,
                       const char* /*hash*/,
                       const char* plugin,
                       std::int64_t input_value,
                       nxd_test::PortIoSample& output) noexcept {
    host::HeadlessHarness harness(plugin, descriptor);
    if (const auto st = harness.load(); !st) {
        std::cerr << "load failed for " << plugin << " code=" << static_cast<int>(st.error()) << "\n";
        return false;
    }
    if (const auto st = harness.initialize(); !st) {
        std::cerr << "initialize failed for " << plugin << "\n";
        return false;
    }

    harness.configure_ports(make_bindings());
    if (const auto st = harness.arm(12345); !st) {
        std::cerr << "arm failed for " << plugin << "\n";
        return false;
    }

    auto in_buf = harness.port_buffer<nxd_test::PortIoSample>("in");
    auto out_buf = harness.port_buffer<nxd_test::PortIoSample>("out");
    if (in_buf.size() != 1 || out_buf.size() != 1) {
        std::cerr << "buffer size mismatch for " << plugin << "\n";
        return false;
    }

    in_buf[0] = nxd_test::PortIoSample{.sequence = 7, .value = input_value};
    if (const auto st = harness.run(1); !st) {
        std::cerr << "run failed for " << plugin << "\n";
        return false;
    }
    output = out_buf[0];
    return true;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0]
                  << " <descriptor.json> <descriptor.hash> <cpp-plugin.so> <rust-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* hash_path = argv[2];
    const char* cpp_plugin = argv[3];
    const char* rust_plugin = argv[4];

    nxd_test::PortIoSample cpp_out{};
    nxd_test::PortIoSample rust_out{};
    if (!run_plugin(descriptor_path, hash_path, cpp_plugin, 42, cpp_out)) {
        return 2;
    }
    if (!run_plugin(descriptor_path, hash_path, rust_plugin, 42, rust_out)) {
        return 3;
    }

    if (cpp_out.sequence != rust_out.sequence || cpp_out.value != rust_out.value) {
        std::cerr << "golden mismatch: cpp=" << cpp_out.sequence << "/" << cpp_out.value
                  << " rust=" << rust_out.sequence << "/" << rust_out.value << "\n";
        return 4;
    }

    if (cpp_out.sequence != 7 || cpp_out.value != 43) {
        std::cerr << "unexpected output: sequence=" << cpp_out.sequence
                  << " value=" << cpp_out.value << "\n";
        return 5;
    }

    std::cout << "ok\n";
    return 0;
}
