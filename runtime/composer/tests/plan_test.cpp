/* Composer Dome Plan serialization tests.
 *
 * Verifies that partition and LET-boundary data types round-trip through JSON
 * without losing information.
 */

#include "nxd/runtime/composer/plan.hpp"

#include <cstdint>
#include <string>

namespace composer = nxd::runtime::composer;

int main() {
    composer::DomePlan plan{};
    plan.version_major = composer::dome_plan_version_major;
    plan.version_minor = composer::dome_plan_version_minor;
    plan.chronos = composer::PlanChronosConfig{
        .mode = 3,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 1,
    };

    plan.partitions.push_back(composer::Partition{
        .id = 1,
        .name = "control",
        .instance_ids = {"pid_1", "mixer_1"},
        .source_name = std::nullopt,
    });
    plan.partitions.push_back(composer::Partition{
        .id = 2,
        .name = "plant",
        .instance_ids = {"plant_1"},
        .source_name = std::nullopt,
    });

    plan.let_boundaries.push_back(composer::LetBoundary{
        .id = "control_to_plant",
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .read_time_ns = 0,
        .write_deadline_ns = 200'000,
    });

    plan.wires.push_back(composer::Wire{
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .let_boundary_id = "control_to_plant",
    });

    plan.processes.push_back(composer::Process{
        .id = "p0",
        .executable = "./nxd_process_control",
        .instance_ids = {"pid_1", "mixer_1", "plant_1"},
    });

    plan.thread_role_assignments.push_back(composer::PlanThreadRoleAssignment{
        .instance_id = "pid_1",
        .role = "control",
        .thread = "t0",
    });

    plan.parameter_assignments.push_back(composer::PlanParameterAssignment{
        .instance_id = "pid_1",
        .name = "kp",
        .value = 10,
    });

    const std::string json = plan.to_json();
    if (json.empty()) return 1;

    const auto parsed = composer::DomePlan::from_json(json);
    if (!parsed.has_value()) return 2;
    if (*parsed != plan) return 3;

    // Intra-partition wire omits the LET boundary id.
    composer::DomePlan direct_plan{};
    direct_plan.partitions.push_back(composer::Partition{
        .id = 1,
        .name = "only",
        .instance_ids = {"a", "b"},
        .source_name = std::nullopt,
    });
    direct_plan.wires.push_back(composer::Wire{
        .source_instance = "a",
        .source_port = "out",
        .target_instance = "b",
        .target_port = "in",
        .let_boundary_id = std::nullopt,
    });

    const auto direct_parsed = composer::DomePlan::from_json(direct_plan.to_json());
    if (!direct_parsed.has_value()) return 4;
    if (*direct_parsed != direct_plan) return 5;

    // Invalid JSON is rejected.
    if (composer::DomePlan::from_json("{not json").has_value()) return 6;

    return 0;
}
