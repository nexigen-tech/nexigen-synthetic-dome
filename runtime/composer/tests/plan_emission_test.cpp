/* Composer Dome Plan emission tests.
 *
 * Verifies that a validated manifest is turned into a Dome Plan containing
 * partitions, LET boundaries, wiring, processes, thread-role assignments, and
 * parameter assignments.
 */

#include "nxd/runtime/composer/manifest.hpp"
#include "nxd/runtime/composer/plan.hpp"
#include "nxd/runtime/composer/plan_emission.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace composer = nxd::runtime::composer;

int main() {
    composer::SystemManifest manifest{};
    manifest.components.push_back(composer::ComponentRef{"pid", "pid.nxdd"});

    composer::Instance pid{};
    pid.id = "pid_1";
    pid.component_name = "pid";
    pid.partition_id = 1;
    pid.parameters.push_back(composer::ParameterAssignment{"kp", 42});
    pid.thread_roles.push_back(composer::ThreadRoleAssignment{"control", "t0"});
    manifest.instances.push_back(std::move(pid));

    manifest.partitions.push_back(composer::ManifestPartition{1, "control"});

    manifest.let_boundaries.push_back(composer::ManifestLetBoundary{
        .id = "l1",
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .read_time_ns = 0,
        .write_deadline_ns = 100'000,
    });

    manifest.wires.push_back(composer::ManifestWire{
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .let_boundary_id = "l1",
    });

    manifest.processes.push_back(composer::ManifestProcess{
        .id = "p0",
        .executable = "./nxd_process",
        .instance_ids = {"pid_1"},
    });

    manifest.chronos = composer::ManifestChronosConfig{
        .mode = 3,
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 1,
    };

    std::unordered_map<std::string, nxd::host::ComponentDescriptor> descriptors;
    const auto plan = composer::emit_plan(manifest, descriptors);

    if (plan.version_major != composer::dome_plan_version_major) return 1;
    if (plan.version_minor != composer::dome_plan_version_minor) return 2;
    if (plan.chronos.mode != 3) return 3;
    if (plan.partitions.size() != 1 || plan.partitions[0].id != 1) return 4;
    if (plan.let_boundaries.size() != 1 || plan.let_boundaries[0].id != "l1") return 5;
    if (plan.wires.size() != 1) return 6;
    if (plan.processes.size() != 1 || plan.processes[0].executable != "./nxd_process") {
        return 7;
    }
    if (plan.thread_role_assignments.size() != 1 ||
        plan.thread_role_assignments[0].role != "control" ||
        plan.thread_role_assignments[0].thread != "t0") {
        return 8;
    }
    if (plan.parameter_assignments.size() != 1 ||
        plan.parameter_assignments[0].name != "kp" ||
        plan.parameter_assignments[0].value != 42) {
        return 9;
    }

    // Round-trip through JSON preserves the emitted plan.
    const auto json = plan.to_json();
    const auto parsed = composer::DomePlan::from_json(json);
    if (!parsed.has_value()) return 10;
    if (*parsed != plan) return 11;

    return 0;
}
