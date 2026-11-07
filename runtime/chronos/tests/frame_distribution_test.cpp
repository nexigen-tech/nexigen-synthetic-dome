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

static bool test_registered_partitions_have_expected_ids() {
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return false;

    auto part_a_result = rt.register_partition(1, "left");
    auto part_b_result = rt.register_partition(2, "right");
    if (!part_a_result.has_value() || !part_b_result.has_value()) return false;

    chronos::Partition* const part_a = *part_a_result;
    chronos::Partition* const part_b = *part_b_result;
    if (part_a->id() != 1 || part_b->id() != 2) return false;

    return true;
}

static bool test_runtime_steps_advance_frame_and_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return false;

    auto part_result = rt.register_partition(1, "left");
    if (!part_result.has_value()) return false;

    if (!ok(rt.arm())) return false;

    for (int i = 0; i < 3; ++i) {
        if (!ok(rt.step())) return false;
        if (rt.frame_no() != static_cast<std::uint64_t>(i) + 1) return false;
        if (rt.sim_time().ns != (i + 1) * base_ns) return false;
    }

    return true;
}

static bool test_partition_latest_context_matches_runtime() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return false;

    auto part_a_result = rt.register_partition(1, "left");
    auto part_b_result = rt.register_partition(2, "right");
    if (!part_a_result.has_value() || !part_b_result.has_value()) return false;

    chronos::Partition* const part_a = *part_a_result;
    chronos::Partition* const part_b = *part_b_result;

    if (!ok(rt.arm())) return false;

    for (int i = 0; i < 3; ++i) {
        if (!ok(rt.step())) return false;
    }

    const auto latest_a = part_a->latest();
    const auto latest_b = part_b->latest();

    if (latest_a.frame_no != latest_b.frame_no) return false;
    if (latest_a.frame_no != 3) return false;
    if (latest_a.sim_time.ns != latest_b.sim_time.ns) return false;
    if (latest_a.sim_time.ns != 3 * base_ns) return false;
    if (latest_a.wall_time.ns != latest_b.wall_time.ns) return false;
    if (latest_a.wall_time.ns != 3 * base_ns) return false;
    if (latest_a.has_hw_fpga_time) return false;
    if (latest_b.has_hw_fpga_time) return false;

    return true;
}

static bool test_register_partition_after_arm_rejected() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return false;
    auto part_result = rt.register_partition(1, "early");
    if (!part_result.has_value()) return false;
    if (!ok(rt.arm())) return false;

    if (rt.register_partition(99, "late").has_value()) return false;
    return true;
}

static bool test_hil_includes_hw_fpga_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime hil{chronos::Config{
        .mode = chronos::Mode::RealTimeHil,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(hil.initialize())) return false;
    auto part_result = hil.register_partition(7, "hil");
    if (!part_result.has_value()) return false;
    chronos::Partition* const part = *part_result;
    if (!ok(hil.arm())) return false;
    if (!ok(hil.step())) return false;
    const auto ctx = part->latest();
    if (!ctx.has_hw_fpga_time) return false;
    if (ctx.hw_fpga_time.ns != base_ns) return false;
    return true;
}

static void reader_thread_fn(chronos::Partition* part,
                             std::int64_t base_ns,
                             std::atomic<bool>* all_consistent,
                             std::atomic<std::uint64_t>* min_frame,
                             std::atomic<std::uint64_t>* max_frame) {
    std::uint64_t last_frame = 0;
    for (int r = 0; r < 1000; ++r) {
        const auto read = part->latest();
        if (read.frame_no < last_frame ||
            read.sim_time.ns != static_cast<std::int64_t>(read.frame_no) * base_ns ||
            read.wall_time.ns != static_cast<std::int64_t>(read.frame_no) * base_ns) {
            all_consistent->store(false, std::memory_order_relaxed);
        }
        last_frame = read.frame_no;

        std::uint64_t observed_min = min_frame->load(std::memory_order_relaxed);
        while (read.frame_no < observed_min &&
               !min_frame->compare_exchange_weak(observed_min, read.frame_no,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed)) {
        }

        std::uint64_t observed_max = max_frame->load(std::memory_order_relaxed);
        while (read.frame_no > observed_max &&
               !max_frame->compare_exchange_weak(observed_max, read.frame_no,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed)) {
        }
    }
}

static bool test_concurrent_readers_observe_consistent_frames() {
    constexpr std::int64_t base_ns = 1'000'000;
    constexpr int kReaders = 8;

    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};

    if (!ok(rt.initialize())) return false;
    auto part_result = rt.register_partition(1, "concurrent");
    if (!part_result.has_value()) return false;
    chronos::Partition* const part = *part_result;
    if (!ok(rt.arm())) return false;

    // Establish some published frames before readers start.
    for (int i = 0; i < 3; ++i) {
        if (!ok(rt.step())) return false;
    }

    std::vector<std::thread> readers;
    readers.reserve(kReaders);
    std::atomic<bool> all_consistent{true};
    std::atomic<std::uint64_t> min_frame{~std::uint64_t{0}};
    std::atomic<std::uint64_t> max_frame{0};

    for (int i = 0; i < kReaders; ++i) {
        readers.emplace_back(reader_thread_fn, part, base_ns, &all_consistent,
                             &min_frame, &max_frame);
    }

    // Exercise the publisher while readers are active.
    for (int i = 0; i < 5; ++i) {
        if (!ok(rt.step())) return false;
    }

    for (auto& t : readers) {
        t.join();
    }

    if (!all_consistent.load(std::memory_order_relaxed)) return false;
    if (max_frame.load(std::memory_order_relaxed) < 3) return false;

    return true;
}

int main() {
    if (!test_registered_partitions_have_expected_ids()) return 1;
    if (!test_runtime_steps_advance_frame_and_time()) return 2;
    if (!test_partition_latest_context_matches_runtime()) return 3;
    if (!test_register_partition_after_arm_rejected()) return 4;
    if (!test_hil_includes_hw_fpga_time()) return 5;
    if (!test_concurrent_readers_observe_consistent_frames()) return 6;

    return 0;
}
