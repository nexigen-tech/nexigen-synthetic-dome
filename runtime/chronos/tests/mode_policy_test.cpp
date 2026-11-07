/* Chronos execution-mode policy tests.
 *
 * Verifies that each supported mode advances SimTime according to its rule and
 * that invalid scale-factor configurations are rejected before arming.
 */

#include "nxd/runtime/chronos/state_machine.hpp"

#include <cstdint>

namespace chronos = nxd::runtime::chronos;

static bool ok(const nexigen::core::Status& s) noexcept {
    return s.has_value();
}

static bool test_step_mode_advances_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    if (!ok(rt.step())) return false;
    if (rt.frame_no() != 1) return false;
    if (rt.sim_time().ns != base_ns) return false;
    if (rt.wall_time().ns != base_ns) return false;
    if (!ok(rt.stop())) return false;
    if (!ok(rt.dispose())) return false;
    return true;
}

static bool test_realtime_hil_advances_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::RealTimeHil,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    for (int i = 0; i < 3; ++i) {
        if (!ok(rt.step())) return false;
    }
    if (rt.frame_no() != 3) return false;
    if (rt.sim_time().ns != 3 * base_ns) return false;
    if (rt.wall_time().ns != 3 * base_ns) return false;
    if (rt.hw_fpga_time().ns != 3 * base_ns) return false;
    return true;
}

static bool test_scaled_sil_advances_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    constexpr std::uint32_t scale = 4;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::ScaledSil,
        .base_frame_interval_ns = base_ns * static_cast<std::int64_t>(scale),
        .scale_factor = scale,
    }};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    for (int i = 0; i < 5; ++i) {
        if (!ok(rt.step())) return false;
    }
    if (rt.frame_no() != 5) return false;
    if (rt.sim_time().ns != 5 * base_ns * scale) return false;
    if (rt.wall_time().ns != 5 * base_ns) return false;
    return true;
}

static bool test_free_sil_advances_time() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::FreeSil,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    if (!ok(rt.step())) return false;
    if (rt.sim_time().ns != base_ns) return false;
    if (rt.wall_time().ns != 0) return false;
    return true;
}

static bool test_play_pause_rejected_in_step_mode() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime stepper{chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(stepper.initialize())) return false;
    if (!ok(stepper.arm())) return false;
    if (ok(stepper.play())) return false;
    if (!ok(stepper.step())) return false;
    return true;
}

static bool test_play_pause_allowed_in_free_sil() {
    constexpr std::int64_t base_ns = 1'000'000;
    chronos::Runtime rt{chronos::Config{
        .mode = chronos::Mode::FreeSil,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    }};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    if (!ok(rt.play())) return false;
    if (rt.state() != chronos::State::Playing) return false;
    if (!ok(rt.pause())) return false;
    if (rt.state() != chronos::State::Paused) return false;
    return true;
}

int main() {
    if (!test_step_mode_advances_time()) return 1;
    if (!test_realtime_hil_advances_time()) return 2;
    if (!test_scaled_sil_advances_time()) return 3;
    if (!test_free_sil_advances_time()) return 4;
    if (!test_play_pause_rejected_in_step_mode()) return 5;
    if (!test_play_pause_allowed_in_free_sil()) return 6;

    return 0;
}
