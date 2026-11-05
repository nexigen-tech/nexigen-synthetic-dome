/* Child-process runner for panic and failure-injection tests.
 *
 * Loads a plugin/descriptor pair, drives the headless harness for the requested
 * number of frames, and prints the result.  Exits 0 when the harness produces
 * any result; a Rust panic plugin aborts the process before reaching exit.
 */

#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0]
                  << " <plugin.so> <descriptor.json> <frame_count> <seed>\n";
        return 1;
    }

    const char* plugin_path = argv[1];
    const char* descriptor_path = argv[2];
    const std::uint64_t frame_count =
        static_cast<std::uint64_t>(std::strtoull(argv[3], nullptr, 10));
    const std::uint64_t seed = static_cast<std::uint64_t>(std::strtoull(argv[4], nullptr, 10));

    nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
    if (const auto st = harness.load(); !st) {
        std::cerr << "child: load failed\n";
        return 1;
    }
    if (const auto st = harness.initialize(); !st) {
        std::cerr << "child: initialize failed\n";
        return 1;
    }
    if (const auto st = harness.arm(seed); !st) {
        std::cerr << "child: arm failed\n";
        return 1;
    }

    // Deliberately ignore the returned status: step failures are part of the
    // observable result and are reported below.
    static_cast<void>(harness.run(frame_count));

    const auto& result = harness.last_result();
    std::cout << "frames_completed=" << result.frames_completed
              << " failed_frame=" << result.failed_frame << " last_status=" << result.last_status
              << " stopped_normally=" << (result.stopped_normally ? 1 : 0) << "\n";
    return 0;
}
