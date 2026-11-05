/* Chronos LET deadline tracker tests.
 *
 * Verifies that late writes to cross-partition ports are detected and recorded
 * while intra-partition writes are not tracked.
 */

#include "nxd/runtime/chronos/let_boundary.hpp"

#include <string>

namespace chronos = nxd::runtime::chronos;

int main() {
    chronos::LetDeadlineTracker tracker;

    tracker.declare_boundary(chronos::LetBoundary{
        .id = "control_to_plant",
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .read_time = chronos::SimTime{0},
        .write_deadline = chronos::SimTime{200'000},
    });

    // On-time cross-partition write.
    {
        const auto violation = tracker.record_write(
            "pid_1", "command", "plant_1", "setpoint", chronos::SimTime{150'000});
        if (violation.has_value()) return 1;
        if (!tracker.violations().empty()) return 2;
    }

    // Late cross-partition write.
    {
        const auto violation = tracker.record_write(
            "pid_1", "command", "plant_1", "setpoint", chronos::SimTime{300'000});
        if (!violation.has_value()) return 10;
        if (violation->lateness_ns != 100'000) return 11;
        if (tracker.violations().size() != 1) return 12;
    }

    // Intra-partition write is not tracked (no boundary declared).
    {
        const auto violation = tracker.record_write(
            "pid_1", "command", "mixer_1", "input", chronos::SimTime{1'000'000});
        if (violation.has_value()) return 20;
        if (tracker.violations().size() != 1) return 21;
    }

    // Reset clears violations.
    tracker.reset();
    if (!tracker.violations().empty()) return 30;

    return 0;
}
