#include "nxd/runtime/composer/validation.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <unordered_map>

namespace nxd::runtime::composer {

namespace {

[[nodiscard]] std::optional<std::uint32_t>
find_partition_id(const DomePlan& plan, const std::string& instance_id) noexcept {
    for (const auto& partition : plan.partitions) {
        for (const auto& member : partition.instance_ids) {
            if (member == instance_id) {
                return partition.id;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] const std::optional<std::string>*
find_source_name(const DomePlan& plan, const std::string& instance_id) noexcept {
    for (const auto& partition : plan.partitions) {
        for (const auto& member : partition.instance_ids) {
            if (member == instance_id) {
                return &partition.source_name;
            }
        }
    }
    return nullptr;
}

} // namespace

std::vector<ValidationDiagnostic>
validate_intra_partition_direct_wiring(const DomePlan& plan) noexcept {
    std::vector<ValidationDiagnostic> diagnostics;

    std::unordered_map<std::string, std::uint32_t> instance_to_partition;
    for (const auto& partition : plan.partitions) {
        for (const auto& instance_id : partition.instance_ids) {
            instance_to_partition[instance_id] = partition.id;
        }
    }

    for (const auto& wire : plan.wires) {
        if (wire.let_boundary_id.has_value()) {
            continue;
        }

        const auto source_partition = find_partition_id(plan, wire.source_instance);
        const auto target_partition = find_partition_id(plan, wire.target_instance);

        const bool same_partition =
            source_partition.has_value() && target_partition.has_value() &&
            *source_partition == *target_partition;

        if (!same_partition) {
            diagnostics.push_back(ValidationDiagnostic{
                .message = std::format(
                    "direct wire from {}.{} to {}.{} crosses partitions without a LET boundary",
                    wire.source_instance, wire.source_port,
                    wire.target_instance, wire.target_port),
                .source_partition_id = source_partition,
                .target_partition_id = target_partition,
                .source_instance = wire.source_instance,
                .source_port = wire.source_port,
                .target_instance = wire.target_instance,
                .target_port = wire.target_port,
                .component_instance = {},
                .component_name = {},
                .contract_name = {},
                .role = {},
            });
        }
    }

    return diagnostics;
}

std::vector<ValidationDiagnostic>
validate_let_boundaries(const DomePlan& plan) noexcept {
    std::vector<ValidationDiagnostic> diagnostics;

    std::unordered_map<std::string, std::uint32_t> instance_to_partition;
    for (const auto& partition : plan.partitions) {
        for (const auto& instance_id : partition.instance_ids) {
            instance_to_partition[instance_id] = partition.id;
        }
    }

    for (const auto& wire : plan.wires) {
        const auto source_partition = find_partition_id(plan, wire.source_instance);
        const auto target_partition = find_partition_id(plan, wire.target_instance);

        const bool same_partition =
            source_partition.has_value() && target_partition.has_value() &&
            *source_partition == *target_partition;

        // Intra-partition wires do not need LET boundaries.
        if (same_partition) {
            continue;
        }

        if (!wire.let_boundary_id.has_value()) {
            diagnostics.push_back(ValidationDiagnostic{
                .message = std::format(
                    "cross-partition wire from {}.{} to {}.{} missing LET boundary",
                    wire.source_instance, wire.source_port,
                    wire.target_instance, wire.target_port),
                .source_partition_id = source_partition,
                .target_partition_id = target_partition,
                .source_instance = wire.source_instance,
                .source_port = wire.source_port,
                .target_instance = wire.target_instance,
                .target_port = wire.target_port,
                .component_instance = {},
                .component_name = {},
                .contract_name = {},
                .role = {},
            });
            continue;
        }

        const auto boundary_it =
            std::ranges::find(plan.let_boundaries, *wire.let_boundary_id,
                              &LetBoundary::id);
        if (boundary_it == plan.let_boundaries.end()) {
            diagnostics.push_back(ValidationDiagnostic{
                .message = std::format(
                    "wire from {}.{} to {}.{} references unknown LET boundary '{}'",
                    wire.source_instance, wire.source_port,
                    wire.target_instance, wire.target_port,
                    *wire.let_boundary_id),
                .source_partition_id = source_partition,
                .target_partition_id = target_partition,
                .source_instance = wire.source_instance,
                .source_port = wire.source_port,
                .target_instance = wire.target_instance,
                .target_port = wire.target_port,
                .component_instance = {},
                .component_name = {},
                .contract_name = {},
                .role = {},
            });
            continue;
        }

        if (boundary_it->source_instance != wire.source_instance ||
            boundary_it->source_port != wire.source_port ||
            boundary_it->target_instance != wire.target_instance ||
            boundary_it->target_port != wire.target_port) {
            diagnostics.push_back(ValidationDiagnostic{
                .message = std::format(
                    "wire from {}.{} to {}.{} does not match LET boundary '{}' endpoints",
                    wire.source_instance, wire.source_port,
                    wire.target_instance, wire.target_port,
                    *wire.let_boundary_id),
                .source_partition_id = source_partition,
                .target_partition_id = target_partition,
                .source_instance = wire.source_instance,
                .source_port = wire.source_port,
                .target_instance = wire.target_instance,
                .target_port = wire.target_port,
                .component_instance = {},
                .component_name = {},
                .contract_name = {},
                .role = {},
            });
        }
    }

    return diagnostics;
}

std::vector<ValidationDiagnostic>
validate_source_name_wiring(const DomePlan& plan) noexcept {
    std::vector<ValidationDiagnostic> diagnostics;

    for (const auto& wire : plan.wires) {
        const auto* source_source_name = find_source_name(plan, wire.source_instance);
        const auto* target_source_name = find_source_name(plan, wire.target_instance);
        if (source_source_name == nullptr || target_source_name == nullptr) {
            continue;
        }
        if (!source_source_name->has_value() || !target_source_name->has_value()) {
            continue;
        }
        if (**source_source_name == **target_source_name) {
            continue;
        }

        diagnostics.push_back(ValidationDiagnostic{
            .message = std::format(
                "wire from {}.{} to {}.{} crosses source names '{}' and '{}'",
                wire.source_instance, wire.source_port,
                wire.target_instance, wire.target_port,
                **source_source_name, **target_source_name),
            .source_partition_id = find_partition_id(plan, wire.source_instance),
            .target_partition_id = find_partition_id(plan, wire.target_instance),
            .source_instance = wire.source_instance,
            .source_port = wire.source_port,
            .target_instance = wire.target_instance,
            .target_port = wire.target_port,
            .component_instance = {},
            .component_name = {},
            .contract_name = {},
            .role = {},
        });
    }

    return diagnostics;
}

} // namespace nxd::runtime::composer
