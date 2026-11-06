#include "nxd/host/harness.hpp"
#include "nxd/contracts/ScenarioCommand.hpp"
#include "nxd/contracts/ScenarioState.hpp"
#include "nxd/contracts/SignalValueU32.hpp"
#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

constexpr std::array<std::uint8_t, 32> scenario_command_hash = {
    0xf8, 0x69, 0xa5, 0x7d, 0xa4, 0x4a, 0xad, 0xee, 0x47, 0x6a, 0xed, 0xaa, 0x94, 0x50, 0x05,
    0x3e, 0xa3, 0x80, 0x08, 0x4e, 0xcc, 0xd7, 0x86, 0x2e, 0x14, 0xad, 0x76, 0xf2, 0x35, 0xce,
    0x55, 0x3a};

constexpr std::array<std::uint8_t, 32> signal_value_u32_hash = {
    0x3e, 0x0d, 0x58, 0xa6, 0x17, 0x7f, 0x41, 0x3c, 0xad, 0xd2, 0xa1, 0x20, 0x48, 0x10, 0xc8,
    0xf9, 0x29, 0x08, 0x12, 0x63, 0x1d, 0x62, 0x88, 0xee, 0x46, 0xfb, 0xa2, 0xa2, 0xbe, 0x23,
    0x3c, 0x2e};

nxd::host::PortBinding make_binding(const char* name,
                                    nxd::host::PortDescriptor::Direction dir,
                                    std::size_t element_size,
                                    const std::array<std::uint8_t, 32>& hash) {
    nxd::host::PortBinding binding;
    binding.name = name;
    binding.direction = dir;
    binding.element_size = element_size;
    binding.count = 1;
    binding.layout_hash = hash;
    binding.buffer.resize(element_size);
    return binding;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cerr << "usage: " << argv[0]
                  << " <scenario_control.descriptor.json> <scenario_control.descriptor.hash>"
                     " <scenario_control.so> <scenario_consumer.descriptor.json>"
                     " <scenario_consumer.descriptor.hash> <scenario_consumer.so>\n";
        return 1;
    }

    nxd_blackboard_initialize();

    nxd::host::HeadlessHarness control_harness(argv[3], argv[1]);
    nxd::host::HeadlessHarness consumer_harness(argv[6], argv[4]);

    if (!control_harness.load()) {
        std::cerr << "failed to load scenario_control\n";
        return 2;
    }
    if (!consumer_harness.load()) {
        std::cerr << "failed to load scenario_consumer\n";
        return 3;
    }

    control_harness.configure_ports({make_binding(
        "commands", nxd::host::PortDescriptor::Direction::In,
        sizeof(nxd::contracts::ScenarioCommand), scenario_command_hash)});
    consumer_harness.configure_ports({make_binding(
        "out", nxd::host::PortDescriptor::Direction::Out,
        sizeof(nxd::contracts::SignalValueU32), signal_value_u32_hash)});

    if (!control_harness.initialize("5:42,10:99")) {
        std::cerr << "failed to initialize scenario_control\n";
        return 4;
    }
    if (!consumer_harness.initialize("")) {
        std::cerr << "failed to initialize scenario_consumer\n";
        return 5;
    }

    if (!control_harness.arm(1)) {
        std::cerr << "failed to arm scenario_control\n";
        return 6;
    }
    if (!consumer_harness.arm(1)) {
        std::cerr << "failed to arm scenario_consumer\n";
        return 7;
    }

    for (std::uint64_t step = 0; step < 12; ++step) {
        const std::uint64_t frame_no = control_harness.frames_run();
        if (!control_harness.run(1)) {
            std::cerr << "scenario_control step failed at frame " << frame_no << "\n";
            return 8;
        }
        if (!consumer_harness.run(1)) {
            std::cerr << "scenario_consumer step failed at frame " << frame_no << "\n";
            return 9;
        }

        auto out_span = consumer_harness.port_buffer<nxd::contracts::SignalValueU32>("out");
        if (out_span.empty()) {
            std::cerr << "missing consumer output at frame " << frame_no << "\n";
            return 10;
        }

        std::uint32_t expected = 0;
        if (frame_no >= 10) {
            expected = 99;
        } else if (frame_no >= 5) {
            expected = 42;
        }

        if (out_span[0].value != expected) {
            std::cerr << "frame " << frame_no << ": expected " << expected << " got "
                      << out_span[0].value << "\n";
            return 11;
        }
    }

    return 0;
}
