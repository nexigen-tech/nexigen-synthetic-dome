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

static composer::SystemManifest make_sample_manifest() {
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

    return manifest;
}

static composer::DomePlan emit_sample_plan() {
    const auto manifest = make_sample_manifest();
    std::unordered_map<std::string, nxd::host::ComponentDescriptor> descriptors;
    return composer::emit_plan(manifest, descriptors);
}

static bool test_emitted_plan_has_expected_header_and_partition() {
    const auto plan = emit_sample_plan();
    if (plan.version_major != composer::dome_plan_version_major) return false;
    if (plan.version_minor != composer::dome_plan_version_minor) return false;
    if (plan.chronos.mode != 3) return false;
    if (plan.partitions.size() != 1 || plan.partitions[0].id != 1) return false;
    return true;
}

static bool test_emitted_plan_has_expected_boundary_and_wires() {
    const auto plan = emit_sample_plan();
    if (plan.let_boundaries.size() != 1 || plan.let_boundaries[0].id != "l1") return false;
    if (plan.wires.size() != 1) return false;
    return true;
}

static bool test_emitted_plan_has_expected_process_and_assignments() {
    const auto plan = emit_sample_plan();
    if (plan.processes.size() != 1 || plan.processes[0].executable != "./nxd_process") {
        return false;
    }
    if (plan.thread_role_assignments.size() != 1 ||
        plan.thread_role_assignments[0].role != "control" ||
        plan.thread_role_assignments[0].thread != "t0") {
        return false;
    }
    if (plan.parameter_assignments.size() != 1 ||
        plan.parameter_assignments[0].name != "kp" ||
        plan.parameter_assignments[0].value != 42) {
        return false;
    }
    return true;
}

static bool test_plan_round_trips_through_json() {
    const auto plan = emit_sample_plan();
    const auto json = plan.to_json();
    const auto parsed = composer::DomePlan::from_json(json);
    if (!parsed.has_value()) return false;
    if (*parsed != plan) return false;
    return true;
}

int main() {
    if (!test_emitted_plan_has_expected_header_and_partition()) return 1;
    if (!test_emitted_plan_has_expected_boundary_and_wires()) return 2;
    if (!test_emitted_plan_has_expected_process_and_assignments()) return 3;
    if (!test_plan_round_trips_through_json()) return 4;

    return 0;
}
