#include "nxd/runtime/composer/plan_emission.hpp"

namespace nxd::runtime::composer {

DomePlan emit_plan(
    const SystemManifest& manifest,
    const std::unordered_map<std::string, nxd::host::ComponentDescriptor>&
    /*descriptors*/) noexcept {
    DomePlan plan;
    plan.version_major = dome_plan_version_major;
    plan.version_minor = dome_plan_version_minor;

    plan.chronos = PlanChronosConfig{
        .mode = manifest.chronos.mode,
        .base_frame_interval_ns = manifest.chronos.base_frame_interval_ns,
        .scale_factor = manifest.chronos.scale_factor,
    };

    for (const auto& p : manifest.partitions) {
        Partition partition;
        partition.id = p.id;
        partition.name = p.name;
        for (const auto& inst : manifest.instances) {
            if (inst.partition_id == p.id) {
                partition.instance_ids.push_back(inst.id);
            }
        }
        plan.partitions.push_back(std::move(partition));
    }

    for (const auto& b : manifest.let_boundaries) {
        LetBoundary boundary;
        boundary.id = b.id;
        boundary.source_instance = b.source_instance;
        boundary.source_port = b.source_port;
        boundary.target_instance = b.target_instance;
        boundary.target_port = b.target_port;
        boundary.read_time_ns = b.read_time_ns;
        boundary.write_deadline_ns = b.write_deadline_ns;
        plan.let_boundaries.push_back(std::move(boundary));
    }

    for (const auto& w : manifest.wires) {
        Wire wire;
        wire.source_instance = w.source_instance;
        wire.source_port = w.source_port;
        wire.target_instance = w.target_instance;
        wire.target_port = w.target_port;
        wire.let_boundary_id = w.let_boundary_id;
        plan.wires.push_back(std::move(wire));
    }

    for (const auto& proc : manifest.processes) {
        Process process;
        process.id = proc.id;
        process.executable = proc.executable;
        process.instance_ids = proc.instance_ids;
        plan.processes.push_back(std::move(process));
    }

    for (const auto& inst : manifest.instances) {
        for (const auto& t : inst.thread_roles) {
            plan.thread_role_assignments.push_back(PlanThreadRoleAssignment{
                .instance_id = inst.id,
                .role = t.role,
                .thread = t.thread,
            });
        }
        for (const auto& p : inst.parameters) {
            plan.parameter_assignments.push_back(PlanParameterAssignment{
                .instance_id = inst.id,
                .name = p.name,
                .value = p.value,
            });
        }
    }

    return plan;
}

} // namespace nxd::runtime::composer
