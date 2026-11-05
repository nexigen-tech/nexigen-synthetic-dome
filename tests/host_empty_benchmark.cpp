/* Empty-component ABI overhead benchmark.
 *
 * Measures the host-mediated plugin boundary overhead for a no-op component.
 * The report identifies the build configuration and measured per-frame call
 * overhead; it does not claim scheduler, transport, or end-to-end latency.
 */

#include "nxd/host/harness.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <empty-plugin.so> <empty-descriptor.json> [frame_count]\n";
        return 1;
    }

    const char* plugin_path = argv[1];
    const char* descriptor_path = argv[2];
    const std::uint64_t frame_count =
        (argc > 3) ? static_cast<std::uint64_t>(std::strtoull(argv[3], nullptr, 10)) : 1'000'000ULL;

    nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
    if (const auto st = harness.load(); !st) {
        std::cerr << "benchmark: load failed\n";
        return 1;
    }
    if (const auto st = harness.initialize(); !st) {
        std::cerr << "benchmark: initialize failed\n";
        return 1;
    }
    if (const auto st = harness.arm(0); !st) {
        std::cerr << "benchmark: arm failed\n";
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    if (const auto st = harness.run(frame_count); !st) {
        std::cerr << "benchmark: run failed\n";
        return 1;
    }
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const double ns_per_frame = static_cast<double>(elapsed_ns) / static_cast<double>(frame_count);

    std::cout << "{"
              << "\"config\":\"" BENCHMARK_CONFIG "\"," << "\"frames\":" << frame_count << ","
              << "\"elapsed_ns\":" << elapsed_ns << ","
              << "\"ns_per_frame\":" << ns_per_frame << ","
              << "\"table_size_bytes\":" << sizeof(NxdAbiTable) << "}"
              << "\n";
    return 0;
}
