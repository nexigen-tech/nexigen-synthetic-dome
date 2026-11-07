/* Chronos runtime state-machine tests.
 *
 * Verifies the allowed lifecycle transitions and, crucially, that every
 * disallowed transition returns an error without mutating runtime state.
 */

#include "nxd/runtime/chronos/state_machine.hpp"

#include <cstdint>
#include <nexigen/core/result.hpp>

namespace chronos = nxd::runtime::chronos;

static bool ok(const nexigen::core::Status& s) noexcept {
    return s.has_value();
}

static bool failed(const nexigen::core::Status& s) noexcept {
    return !s.has_value();
}

static chronos::Config step_config() {
    return chronos::Config{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 1,
    };
}

static chronos::Config scaled_config(std::uint32_t scale) {
    return chronos::Config{
        .mode = chronos::Mode::ScaledSil,
        .base_frame_interval_ns = static_cast<std::int64_t>(scale) * 1'000'000,
        .scale_factor = scale,
    };
}

static bool test_step_lifecycle_happy_path() {
    chronos::Runtime rt(step_config());
    if (rt.state() != chronos::State::Uninitialized) return false;
    if (!ok(rt.initialize())) return false;
    if (rt.state() != chronos::State::Initialized) return false;
    if (!ok(rt.arm())) return false;
    if (rt.state() != chronos::State::Armed) return false;
    if (rt.frame_no() != 0) return false;
    if (!ok(rt.step())) return false;
    if (rt.state() != chronos::State::Armed) return false;
    if (rt.frame_no() != 1) return false;
    if (rt.sim_time().ns != 1'000'000) return false;
    if (!ok(rt.stop())) return false;
    if (rt.state() != chronos::State::Stopped) return false;
    if (!ok(rt.dispose())) return false;
    if (rt.state() != chronos::State::Disposed) return false;
    return true;
}

static bool test_continuous_mode_lifecycle() {
    chronos::Runtime rt{scaled_config(2)};
    if (!ok(rt.initialize())) return false;
    if (!ok(rt.arm())) return false;
    if (!ok(rt.play())) return false;
    if (rt.state() != chronos::State::Playing) return false;
    if (!ok(rt.pause())) return false;
    if (rt.state() != chronos::State::Paused) return false;
    if (!ok(rt.play())) return false;
    if (rt.state() != chronos::State::Playing) return false;
    if (!ok(rt.stop())) return false;
    if (rt.state() != chronos::State::Stopped) return false;
    return true;
}

static bool test_rejected_transactions_preserve_state() {
    chronos::Runtime rt{step_config()};
    const auto before = rt.state();
    if (!failed(rt.arm())) return false; // uninitialized -> arm invalid
    if (rt.state() != before) return false;

    if (!ok(rt.initialize())) return false;
    if (!failed(rt.initialize())) return false; // already initialized
    if (rt.state() != chronos::State::Initialized) return false;

    if (!failed(rt.play())) return false; // Step mode rejects play
    if (rt.state() != chronos::State::Initialized) return false;

    if (!failed(rt.step())) return false; // initialized -> step invalid
    if (rt.state() != chronos::State::Initialized) return false;

    if (!ok(rt.arm())) return false;
    auto state_after_arm = rt.state();
    if (!failed(rt.arm())) return false; // already armed
    if (rt.state() != state_after_arm) return false;

    if (!ok(rt.step())) return false;
    if (rt.state() != chronos::State::Armed) return false;

    // Cannot pause from Armed (must be Playing).
    if (!failed(rt.pause())) return false;
    if (rt.state() != chronos::State::Armed) return false;

    if (!ok(rt.stop())) return false;
    if (rt.state() != chronos::State::Stopped) return false;

    // Disallowed after stop.
    if (!failed(rt.play())) return false;
    if (rt.state() != chronos::State::Stopped) return false;
    if (!failed(rt.step())) return false;
    if (rt.state() != chronos::State::Stopped) return false;
    if (!failed(rt.pause())) return false;
    if (rt.state() != chronos::State::Stopped) return false;

    if (!ok(rt.dispose())) return false;
    if (rt.state() != chronos::State::Disposed) return false;
    if (!failed(rt.dispose())) return false; // already disposed
    if (rt.state() != chronos::State::Disposed) return false;

    return true;
}

static bool test_invalid_config_non_divisible_scaled_sil() {
    chronos::Config cfg{
        .mode = chronos::Mode::ScaledSil,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 3,
    };
    chronos::Runtime rt{cfg};
    if (!failed(rt.initialize())) return false;
    if (rt.state() != chronos::State::Uninitialized) return false;
    return true;
}

static bool test_invalid_config_zero_base_interval() {
    chronos::Config cfg{
        .mode = chronos::Mode::Step,
        .base_frame_interval_ns = 0,
        .scale_factor = 1,
    };
    chronos::Runtime rt{cfg};
    if (!failed(rt.initialize())) return false;
    return true;
}

static bool test_invalid_config_zero_scale_factor() {
    chronos::Config cfg{
        .mode = chronos::Mode::ScaledSil,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 0,
    };
    chronos::Runtime rt{cfg};
    if (!failed(rt.initialize())) return false;
    return true;
}

int main() {
    if (!test_step_lifecycle_happy_path()) return 1;
    if (!test_continuous_mode_lifecycle()) return 2;
    if (!test_rejected_transactions_preserve_state()) return 3;
    if (!test_invalid_config_non_divisible_scaled_sil()) return 4;
    if (!test_invalid_config_zero_base_interval()) return 5;
    if (!test_invalid_config_zero_scale_factor()) return 6;

    return 0;
}
