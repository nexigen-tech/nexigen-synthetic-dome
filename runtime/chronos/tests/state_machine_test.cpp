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

int main() {
    // Happy path: initialize -> arm -> step -> stop -> dispose.
    {
        chronos::Runtime rt(step_config());
        if (rt.state() != chronos::State::Uninitialized) return 1;
        if (!ok(rt.initialize())) return 2;
        if (rt.state() != chronos::State::Initialized) return 3;
        if (!ok(rt.arm())) return 4;
        if (rt.state() != chronos::State::Armed) return 5;
        if (rt.frame_no() != 0) return 6;
        if (!ok(rt.step())) return 7;
        if (rt.state() != chronos::State::Armed) return 8;
        if (rt.frame_no() != 1) return 9;
        if (rt.sim_time().ns != 1'000'000) return 10;
        if (!ok(rt.stop())) return 11;
        if (rt.state() != chronos::State::Stopped) return 12;
        if (!ok(rt.dispose())) return 13;
        if (rt.state() != chronos::State::Disposed) return 14;
    }

    // Continuous-mode lifecycle: initialize -> arm -> play -> pause -> play -> stop.
    {
        chronos::Runtime rt{scaled_config(2)};
        if (!ok(rt.initialize())) return 20;
        if (!ok(rt.arm())) return 21;
        if (!ok(rt.play())) return 22;
        if (rt.state() != chronos::State::Playing) return 23;
        if (!ok(rt.pause())) return 24;
        if (rt.state() != chronos::State::Paused) return 25;
        if (!ok(rt.play())) return 26;
        if (rt.state() != chronos::State::Playing) return 27;
        if (!ok(rt.stop())) return 28;
        if (rt.state() != chronos::State::Stopped) return 29;
    }

    // Rejected transitions must not change state.
    {
        chronos::Runtime rt{step_config()};
        const auto before = rt.state();
        if (!failed(rt.arm())) return 30;       // uninitialized -> arm invalid
        if (rt.state() != before) return 31;

        if (!ok(rt.initialize())) return 32;
        if (!failed(rt.initialize())) return 33; // already initialized
        if (rt.state() != chronos::State::Initialized) return 34;

        if (!failed(rt.play())) return 35;       // Step mode rejects play
        if (rt.state() != chronos::State::Initialized) return 36;

        if (!failed(rt.step())) return 37;       // initialized -> step invalid
        if (rt.state() != chronos::State::Initialized) return 38;

        if (!ok(rt.arm())) return 39;
        auto state_after_arm = rt.state();
        if (!failed(rt.arm())) return 40;       // already armed
        if (rt.state() != state_after_arm) return 41;

        if (!ok(rt.step())) return 42;
        if (rt.state() != chronos::State::Armed) return 43;

        // Cannot pause from Armed (must be Playing).
        if (!failed(rt.pause())) return 44;
        if (rt.state() != chronos::State::Armed) return 45;

        if (!ok(rt.stop())) return 46;
        if (rt.state() != chronos::State::Stopped) return 47;

        // Disallowed after stop.
        if (!failed(rt.play())) return 48;
        if (rt.state() != chronos::State::Stopped) return 49;
        if (!failed(rt.step())) return 50;
        if (rt.state() != chronos::State::Stopped) return 51;
        if (!failed(rt.pause())) return 52;
        if (rt.state() != chronos::State::Stopped) return 53;

        if (!ok(rt.dispose())) return 54;
        if (rt.state() != chronos::State::Disposed) return 55;
        if (!failed(rt.dispose())) return 56; // already disposed
        if (rt.state() != chronos::State::Disposed) return 57;
    }

    // Invalid configuration: non-divisible ScaledSil interval.
    {
        chronos::Config cfg{
            .mode = chronos::Mode::ScaledSil,
            .base_frame_interval_ns = 1'000'000,
            .scale_factor = 3,
        };
        chronos::Runtime rt{cfg};
        if (!failed(rt.initialize())) return 58;
        if (rt.state() != chronos::State::Uninitialized) return 59;
    }

    // Invalid configuration: zero base interval or scale.
    {
        chronos::Config cfg{
            .mode = chronos::Mode::Step,
            .base_frame_interval_ns = 0,
            .scale_factor = 1,
        };
        chronos::Runtime rt{cfg};
        if (!failed(rt.initialize())) return 60;
    }
    {
        chronos::Config cfg{
            .mode = chronos::Mode::ScaledSil,
            .base_frame_interval_ns = 1'000'000,
            .scale_factor = 0,
        };
        chronos::Runtime rt{cfg};
        if (!failed(rt.initialize())) return 61;
    }

    return 0;
}
