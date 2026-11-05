/* Composer direct-wiring validation tests.
 *
 * Verifies that direct connections without a LET boundary are accepted only
 * inside a partition and rejected when they span partitions.
 */

#include "nxd/runtime/composer/plan.hpp"
#include "nxd/runtime/composer/validation.hpp"

#include <cstdint>

namespace composer = nxd::runtime::composer;

int main() {
    // Valid: intra-partition direct wire.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1", "mixer_1"},
            .source_name = std::nullopt,
        });
        plan.wires.push_back(composer::Wire{
            .source_instance = "pid_1",
            .source_port = "command",
            .target_instance = "mixer_1",
            .target_port = "input",
            .let_boundary_id = std::nullopt,
        });

        const auto diagnostics = composer::validate_intra_partition_direct_wiring(plan);
        if (!diagnostics.empty()) return 1;
    }

    // Invalid: cross-partition direct wire without LET boundary.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
            .source_name = std::nullopt,
        });
        plan.partitions.push_back(composer::Partition{
            .id = 2,
            .name = "plant",
            .instance_ids = {"plant_1"},
            .source_name = std::nullopt,
        });
        plan.wires.push_back(composer::Wire{
            .source_instance = "pid_1",
            .source_port = "command",
            .target_instance = "plant_1",
            .target_port = "setpoint",
            .let_boundary_id = std::nullopt,
        });

        const auto diagnostics = composer::validate_intra_partition_direct_wiring(plan);
        if (diagnostics.size() != 1) return 10;
        if (diagnostics[0].source_partition_id != 1) return 11;
        if (diagnostics[0].target_partition_id != 2) return 12;
    }

    // Valid: cross-partition wire with explicit LET boundary.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
            .source_name = std::nullopt,
        });
        plan.partitions.push_back(composer::Partition{
            .id = 2,
            .name = "plant",
            .instance_ids = {"plant_1"},
            .source_name = std::nullopt,
        });
        plan.wires.push_back(composer::Wire{
            .source_instance = "pid_1",
            .source_port = "command",
            .target_instance = "plant_1",
            .target_port = "setpoint",
            .let_boundary_id = "control_to_plant",
        });

        const auto diagnostics = composer::validate_intra_partition_direct_wiring(plan);
        if (!diagnostics.empty()) return 20;
    }

    // Invalid: target instance is not in any partition.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
            .source_name = std::nullopt,
        });
        plan.wires.push_back(composer::Wire{
            .source_instance = "pid_1",
            .source_port = "command",
            .target_instance = "orphan",
            .target_port = "input",
            .let_boundary_id = std::nullopt,
        });

        const auto diagnostics = composer::validate_intra_partition_direct_wiring(plan);
        if (diagnostics.size() != 1) return 30;
        if (diagnostics[0].source_partition_id != 1) return 31;
        if (diagnostics[0].target_partition_id.has_value()) return 32;
    }

    // Cross-partition LET-boundary validation.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
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

        const auto diagnostics = composer::validate_let_boundaries(plan);
        if (!diagnostics.empty()) return 40;
    }

    // Unknown LET boundary id on cross-partition wire.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
            .source_name = std::nullopt,
        });
        plan.partitions.push_back(composer::Partition{
            .id = 2,
            .name = "plant",
            .instance_ids = {"plant_1"},
            .source_name = std::nullopt,
        });
        plan.wires.push_back(composer::Wire{
            .source_instance = "pid_1",
            .source_port = "command",
            .target_instance = "plant_1",
            .target_port = "setpoint",
            .let_boundary_id = "missing",
        });

        const auto diagnostics = composer::validate_let_boundaries(plan);
        if (diagnostics.size() != 1) return 50;
    }

    // LET boundary endpoints do not match wire.
    {
        composer::DomePlan plan;
        plan.partitions.push_back(composer::Partition{
            .id = 1,
            .name = "control",
            .instance_ids = {"pid_1"},
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
            .source_port = "alt_command",
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

        const auto diagnostics = composer::validate_let_boundaries(plan);
        if (diagnostics.size() != 1) return 60;
    }

    return 0;
}
