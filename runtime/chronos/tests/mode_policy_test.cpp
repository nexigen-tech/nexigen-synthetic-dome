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

int main() {
    constexpr std::int64_t base_ns = 1'000'000;

    // Step mode advances logical and wall time by one base frame.
    {
        chronos::Runtime rt{chronos::Config{
            .mode = chronos::Mode::Step,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(rt.initialize())) return 1;
        if (!ok(rt.arm())) return 2;
        if (!ok(rt.step())) return 3;
        if (rt.frame_no() != 1) return 4;
        if (rt.sim_time().ns != base_ns) return 5;
        if (rt.wall_time().ns != base_ns) return 6;
        if (!ok(rt.stop())) return 7;
        if (!ok(rt.dispose())) return 8;
    }

    // RealTimeHil advances SimTime, WallTime, and HwFpgaTime by base frame.
    {
        chronos::Runtime rt{chronos::Config{
            .mode = chronos::Mode::RealTimeHil,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(rt.initialize())) return 10;
        if (!ok(rt.arm())) return 11;
        for (int i = 0; i < 3; ++i) {
            if (!ok(rt.step())) return 12;
        }
        if (rt.frame_no() != 3) return 13;
        if (rt.sim_time().ns != 3 * base_ns) return 14;
        if (rt.wall_time().ns != 3 * base_ns) return 15;
        if (rt.hw_fpga_time().ns != 3 * base_ns) return 16;
    }

    // ScaledSil advances SimTime by base interval, WallTime by base/scale.
    {
        constexpr std::uint32_t scale = 4;
        chronos::Runtime rt{chronos::Config{
            .mode = chronos::Mode::ScaledSil,
            .base_frame_interval_ns = base_ns * static_cast<std::int64_t>(scale),
            .scale_factor = scale,
        }};
        if (!ok(rt.initialize())) return 20;
        if (!ok(rt.arm())) return 21;
        for (int i = 0; i < 5; ++i) {
            if (!ok(rt.step())) return 22;
        }
        if (rt.frame_no() != 5) return 23;
        if (rt.sim_time().ns != 5 * base_ns * scale) return 24;
        if (rt.wall_time().ns != 5 * base_ns) return 25;
    }

    // FreeSil advances SimTime without a wall-clock target.
    {
        chronos::Runtime rt{chronos::Config{
            .mode = chronos::Mode::FreeSil,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(rt.initialize())) return 30;
        if (!ok(rt.arm())) return 31;
        if (!ok(rt.step())) return 32;
        if (rt.sim_time().ns != base_ns) return 33;
        if (rt.wall_time().ns != 0) return 34;
    }

    // play/pause lifecycle is rejected in Step mode and allowed otherwise.
    {
        chronos::Runtime stepper{chronos::Config{
            .mode = chronos::Mode::Step,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(stepper.initialize())) return 40;
        if (!ok(stepper.arm())) return 41;
        if (ok(stepper.play())) return 42;
        if (!ok(stepper.step())) return 43;
    }
    {
        chronos::Runtime rt{chronos::Config{
            .mode = chronos::Mode::FreeSil,
            .base_frame_interval_ns = base_ns,
            .scale_factor = 1,
        }};
        if (!ok(rt.initialize())) return 50;
        if (!ok(rt.arm())) return 51;
        if (!ok(rt.play())) return 52;
        if (rt.state() != chronos::State::Playing) return 53;
        if (!ok(rt.pause())) return 54;
        if (rt.state() != chronos::State::Paused) return 55;
    }

    return 0;
}
