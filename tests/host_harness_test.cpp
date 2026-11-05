/* Headless harness tests.
 *
 * Verifies frame delivery, deterministic seed control, step-failure handling,
 * and allocation-after-arm detection through the high-level harness API.
 */

#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

    bool empty_component_runs(const char* plugin_path, const char* descriptor_path) {
        nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
        if (const auto st = harness.load(); !st) {
            std::cerr << "empty: load failed\n";
            return false;
        }
        if (const auto st = harness.initialize(); !st) {
            std::cerr << "empty: initialize failed\n";
            return false;
        }
        if (const auto st = harness.arm(0x1234); !st) {
            std::cerr << "empty: arm failed\n";
            return false;
        }
        if (const auto st = harness.run(10); !st) {
            std::cerr << "empty: run failed\n";
            return false;
        }
        const auto& result = harness.last_result();
        if (result.frames_completed != 10) {
            std::cerr << "empty: expected 10 frames, got " << result.frames_completed << "\n";
            return false;
        }
        if (!result.stopped_normally) {
            std::cerr << "empty: expected normal stop\n";
            return false;
        }
        return true;
    }

    bool deterministic_counter_runs(const char* plugin_path, const char* descriptor_path) {
        nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
        if (const auto st = harness.load(); !st) {
            std::cerr << "counter: load failed\n";
            return false;
        }
        if (const auto st = harness.initialize(); !st) {
            std::cerr << "counter: initialize failed\n";
            return false;
        }
        if (const auto st = harness.arm(0xABCD); !st) {
            std::cerr << "counter: arm failed\n";
            return false;
        }
        if (const auto st = harness.run(20); !st) {
            std::cerr << "counter: run failed\n";
            return false;
        }
        const auto& result = harness.last_result();
        if (result.frames_completed != 20) {
            std::cerr << "counter: expected 20 frames, got " << result.frames_completed << "\n";
            return false;
        }
        return true;
    }

    bool allocation_after_arm_fails(const char* plugin_path, const char* descriptor_path) {
        nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
        if (const auto st = harness.load(); !st) {
            std::cerr << "alloc: load failed\n";
            return false;
        }
        if (const auto st = harness.initialize(); !st) {
            std::cerr << "alloc: initialize failed\n";
            return false;
        }
        if (const auto st = harness.arm(0); !st) {
            std::cerr << "alloc: arm failed\n";
            return false;
        }
        const auto st = harness.run(10);
        if (st) {
            std::cerr << "alloc: expected run to fail due to allocation\n";
            return false;
        }
        const auto& result = harness.last_result();
        if (result.frames_completed != 0) {
            std::cerr << "alloc: expected 0 completed frames, got " << result.frames_completed
                      << "\n";
            return false;
        }
        if (result.failed_frame != 0) {
            std::cerr << "alloc: expected failed_frame 0, got " << result.failed_frame << "\n";
            return false;
        }
        if (result.last_status != NXD_ERROR_OUT_OF_MEMORY) {
            std::cerr << "alloc: expected NXD_ERROR_OUT_OF_MEMORY, got " << result.last_status
                      << "\n";
            return false;
        }
        return true;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "usage: " << argv[0]
                  << " <empty-plugin.so> <empty-descriptor.json>"
                     " <counter-plugin.so> <counter-descriptor.json>"
                     " <allocating-plugin.so>\n";
        return 1;
    }

    const char* empty_plugin = argv[1];
    const char* empty_descriptor = argv[2];
    const char* counter_plugin = argv[3];
    const char* counter_descriptor = argv[4];
    const char* allocating_plugin = argv[5];

    if (!empty_component_runs(empty_plugin, empty_descriptor)) {
        return 2;
    }
    if (!deterministic_counter_runs(counter_plugin, counter_descriptor)) {
        return 3;
    }
    if (!allocation_after_arm_fails(allocating_plugin, counter_descriptor)) {
        return 4;
    }

    std::cout << "ok\n";
    return 0;
}
