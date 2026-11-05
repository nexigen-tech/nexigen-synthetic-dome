/* Chronos frame-context distribution tests.
 *
 * Verifies that every partition receives the same frame number and time values
 * for a given frame and that reading a partition after stepping returns the
 * latest published context.
 */

#include "nxd/runtime/chronos/state_machine.hpp"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace chronos = nxd::runtime::chronos;

static bool ok(const nexigen::core::Status& s) noexcept {
    return s.has_value();
}

int main() {
    constexpr std::int64_t base_ns = 1'000'000;

    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return 1;

    auto part_a_result = rt.register_partition(1, "left");
    auto part_b_result = rt.register_partition(2, "right");
    if (!part_a_result.has_value() || !part_b_result.has_value()) return 2;

    chronos::Partition* const part_a = *part_a_result;
    chronos::Partition* const part_b = *part_b_result;
    if (part_a->id() != 1 || part_b->id() != 2) return 3;

    if (!ok(rt.arm())) return 4;

    for (int i = 0; i < 3; ++i) {
        if (!ok(rt.step())) return 10 + i;
        if (rt.frame_no() != static_cast<std::uint64_t>(i + 1)) return 20 + i;
        if (rt.sim_time().ns != (i + 1) * base_ns) return 30 + i;
    }

    const auto latest_a = part_a->latest();
    const auto latest_b = part_b->latest();

    if (latest_a.frame_no != latest_b.frame_no) return 40;
    if (latest_a.frame_no != 3) return 41;
    if (latest_a.sim_time.ns != latest_b.sim_time.ns) return 42;
    if (latest_a.sim_time.ns != 3 * base_ns) return 43;
    if (latest_a.wall_time.ns != latest_b.wall_time.ns) return 44;
    if (latest_a.wall_time.ns != 3 * base_ns) return 45;
    if (latest_a.has_hw_fpga_time) return 46;
    if (latest_b.has_hw_fpga_time) return 47;

    // Registration after arming must be rejected.
    if (rt.register_partition(99, "late").has_value()) return 48;

    // HIL mode includes hardware time.
    {
        chronos::Runtime hil{chronos::Config{
            .mode = chronos::Mode::RealTimeHil,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(hil.initialize())) return 50;
        auto part_result = hil.register_partition(7, "hil");
        if (!part_result.has_value()) return 51;
        chronos::Partition* const part = *part_result;
        if (!ok(hil.arm())) return 52;
        if (!ok(hil.step())) return 53;
        const auto ctx = part->latest();
        if (!ctx.has_hw_fpga_time) return 54;
        if (ctx.hw_fpga_time.ns != base_ns) return 55;
    }

    // Concurrent readers on the same partition should observe internally
    // consistent frame contexts: for any observed frame, sim_time and
    // wall_time match frame_no * base_interval, and frames never go backward.
    {
        constexpr int kReaders = 8;

        std::vector<std::thread> readers;
        readers.reserve(kReaders);
        std::atomic<bool> all_consistent{true};
        std::atomic<std::uint64_t> min_frame{~std::uint64_t{0}};
        std::atomic<std::uint64_t> max_frame{0};

        for (int i = 0; i < kReaders; ++i) {
            readers.emplace_back([part_a, &all_consistent, &min_frame, &max_frame]() {
                std::uint64_t last_frame = 0;
                for (int r = 0; r < 1000; ++r) {
                    const auto read = part_a->latest();
                    if (read.frame_no < last_frame ||
                        read.sim_time.ns != static_cast<std::int64_t>(read.frame_no) * base_ns ||
                        read.wall_time.ns != static_cast<std::int64_t>(read.frame_no) * base_ns) {
                        all_consistent.store(false, std::memory_order_relaxed);
                    }
                    last_frame = read.frame_no;

                    std::uint64_t observed_min = min_frame.load(std::memory_order_relaxed);
                    while (read.frame_no < observed_min &&
                           !min_frame.compare_exchange_weak(observed_min, read.frame_no,
                                                            std::memory_order_relaxed,
                                                            std::memory_order_relaxed)) {
                    }

                    std::uint64_t observed_max = max_frame.load(std::memory_order_relaxed);
                    while (read.frame_no > observed_max &&
                           !max_frame.compare_exchange_weak(observed_max, read.frame_no,
                                                            std::memory_order_relaxed,
                                                            std::memory_order_relaxed)) {
                    }
                }
            });
        }

        // Exercise the publisher while readers are active.
        for (int i = 0; i < 5; ++i) {
            if (!ok(rt.step())) return 70 + i;
        }

        for (auto& t : readers) {
            t.join();
        }

        if (!all_consistent.load(std::memory_order_relaxed)) return 60;
        if (max_frame.load(std::memory_order_relaxed) < 3) return 61;
    }

    return 0;
}
