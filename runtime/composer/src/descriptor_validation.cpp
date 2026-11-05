#include "nxd/runtime/composer/descriptor_validation.hpp"

#include "nxd/runtime/composer/manifest.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <unordered_set>

namespace nxd::runtime::composer {

namespace {

[[nodiscard]] ValidationDiagnostic make_diagnostic(
    std::string message,
    std::optional<std::uint32_t> source_partition_id = std::nullopt,
    std::optional<std::uint32_t> target_partition_id = std::nullopt,
    std::string source_instance = {},
    std::string source_port = {},
    std::string target_instance = {},
    std::string target_port = {},
    std::string component_instance = {},
    std::string component_name = {},
    std::string contract_name = {},
    std::string role = {}) noexcept {
    return ValidationDiagnostic{
        .message = std::move(message),
        .source_partition_id = source_partition_id,
        .target_partition_id = target_partition_id,
        .source_instance = std::move(source_instance),
        .source_port = std::move(source_port),
        .target_instance = std::move(target_instance),
        .target_port = std::move(target_port),
        .component_instance = std::move(component_instance),
        .component_name = std::move(component_name),
        .contract_name = std::move(contract_name),
        .role = std::move(role),
    };
}

[[nodiscard]] std::string mode_to_string(std::uint8_t mode) noexcept {
    switch (mode) {
        case 0:
            return "realtime_hil";
        case 1:
            return "scaled_sil";
        case 2:
            return "free_sil";
        case 3:
            return "step";
        default:
            return "unknown";
    }
}

[[nodiscard]] bool is_output_direction(nxd::host::PortDescriptor::Direction dir) noexcept {
    return dir == nxd::host::PortDescriptor::Direction::Out ||
           dir == nxd::host::PortDescriptor::Direction::InOut;
}

[[nodiscard]] bool is_input_direction(nxd::host::PortDescriptor::Direction dir) noexcept {
    return dir == nxd::host::PortDescriptor::Direction::In ||
           dir == nxd::host::PortDescriptor::Direction::InOut;
}

[[nodiscard]] const nxd::host::ComponentDescriptor*
find_descriptor(const std::unordered_map<std::string, nxd::host::ComponentDescriptor>& descriptors,
                const std::string& name) noexcept {
    const auto it = descriptors.find(name);
    if (it == descriptors.end()) {
        return nullptr;
    }
    return &it->second;
}

[[nodiscard]] const nxd::host::PortDescriptor*
find_port(const nxd::host::ComponentDescriptor& descriptor,
          const std::string& port_name) noexcept {
    for (const auto& port : descriptor.ports) {
        if (port.name == port_name) {
            return &port;
        }
    }
    return nullptr;
}

[[nodiscard]] bool port_is_wired(const SystemManifest& manifest,
                                   const Instance& instance,
                                   const nxd::host::PortDescriptor& port) noexcept {
    for (const auto& wire : manifest.wires) {
        if (port.direction == nxd::host::PortDescriptor::Direction::In) {
            if (wire.target_instance == instance.id && wire.target_port == port.name) {
                return true;
            }
        } else if (port.direction == nxd::host::PortDescriptor::Direction::Out) {
            if (wire.source_instance == instance.id && wire.source_port == port.name) {
                return true;
            }
        } else { // InOut
            if ((wire.source_instance == instance.id && wire.source_port == port.name) ||
                (wire.target_instance == instance.id && wire.target_port == port.name)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] const nxd::host::ContractRef*
find_contract(const nxd::host::ComponentDescriptor& descriptor,
              const std::string& contract_name,
              std::uint64_t major_version) noexcept {
    for (const auto& ref : descriptor.contracts) {
        if (ref.name == contract_name && ref.version.major == major_version) {
            return &ref;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<std::string>
contracts_incompatibility(const nxd::host::PortDescriptor& source,
                          const nxd::host::ComponentDescriptor* source_descriptor,
                          const nxd::host::PortDescriptor& target,
                          const nxd::host::ComponentDescriptor* target_descriptor) noexcept {
    if (source.contract_name != target.contract_name) {
        return std::format(
            "contract name mismatch ('{}' vs '{}')",
            source.contract_name, target.contract_name);
    }
    if (source.contract_version.major != target.contract_version.major) {
        return std::format(
            "major version mismatch (v{} vs v{})",
            source.contract_version.major, target.contract_version.major);
    }

    const auto* source_ref = source_descriptor
                                 ? find_contract(
                                       *source_descriptor,
                                       source.contract_name,
                                       source.contract_version.major)
                                 : nullptr;
    const auto* target_ref = target_descriptor
                                 ? find_contract(
                                       *target_descriptor,
                                       target.contract_name,
                                       target.contract_version.major)
                                 : nullptr;
    if (source_ref && source_ref->hash.has_value() && target_ref &&
        target_ref->hash.has_value()) {
        if (*source_ref->hash != *target_ref->hash) {
            return "layout hash mismatch";
        }
    }
    return std::nullopt;
}

void validate_instance(const SystemManifest& manifest,
                           const Instance& instance,
                           const nxd::host::ComponentDescriptor& descriptor,
                           std::vector<ValidationDiagnostic>& diagnostics) noexcept {
    // Required ports.
    for (const auto& port : descriptor.ports) {
        if (!port.required) {
            continue;
        }
        if (!port_is_wired(manifest, instance, port)) {
            diagnostics.push_back(make_diagnostic(
                std::format(
                    "instance '{}' of component '{}' missing required binding for port '{}'",
                    instance.id, descriptor.name, port.name),
                std::nullopt, std::nullopt, "", "", "", "", instance.id, descriptor.name));
        }
    }

    // Required parameters and ranges.
    for (const auto& param : descriptor.parameters) {
        const auto assigned = std::ranges::find(
            instance.parameters, param.name, &ParameterAssignment::name);

        if (param.required && assigned == instance.parameters.end()) {
            diagnostics.push_back(make_diagnostic(
                std::format(
                    "instance '{}' of component '{}' missing required parameter '{}'",
                    instance.id, descriptor.name, param.name),
                std::nullopt, std::nullopt, "", "", "", "", instance.id, descriptor.name));
            continue;
        }

        if (assigned != instance.parameters.end()) {
            if (param.min.has_value() && assigned->value < *param.min) {
                diagnostics.push_back(make_diagnostic(
                    std::format(
                        "instance '{}' parameter '{}' value {} below minimum {}",
                        instance.id, param.name, assigned->value, *param.min),
                    std::nullopt, std::nullopt, "", "", "", "", instance.id,
                    descriptor.name));
            }
            if (param.max.has_value() && assigned->value > *param.max) {
                diagnostics.push_back(make_diagnostic(
                    std::format(
                        "instance '{}' parameter '{}' value {} above maximum {}",
                        instance.id, param.name, assigned->value, *param.max),
                    std::nullopt, std::nullopt, "", "", "", "", instance.id,
                    descriptor.name));
            }
        }
    }

    // Required thread roles.
    for (const auto& role : descriptor.thread_roles) {
        if (!role.required) {
            continue;
        }
        const auto assigned = std::ranges::find(
            instance.thread_roles, role.name, &ThreadRoleAssignment::role);
        if (assigned == instance.thread_roles.end()) {
            diagnostics.push_back(make_diagnostic(
                std::format(
                    "instance '{}' of component '{}' missing required thread role '{}'",
                    instance.id, descriptor.name, role.name),
                std::nullopt, std::nullopt, "", "", "", "", instance.id, descriptor.name,
                "", role.name));
        }
    }

    // Supported modes.
    const std::string mode_name = mode_to_string(manifest.chronos.mode);
    const auto supported = std::ranges::find(descriptor.supported_modes, mode_name);
    if (supported == descriptor.supported_modes.end()) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "instance '{}' of component '{}' does not support chronos mode '{}'",
                instance.id, descriptor.name, mode_name),
            std::nullopt, std::nullopt, "", "", "", "", instance.id, descriptor.name));
    }
}

void validate_wire(const SystemManifest& manifest,
                       const ManifestWire& wire,
                       const std::unordered_map<std::string, nxd::host::ComponentDescriptor>&
                           descriptors,
                       std::vector<ValidationDiagnostic>& diagnostics) noexcept {
    const auto source_instance = std::ranges::find(
        manifest.instances, wire.source_instance, &Instance::id);
    const auto target_instance = std::ranges::find(
        manifest.instances, wire.target_instance, &Instance::id);

    if (source_instance == manifest.instances.end()) {
        diagnostics.push_back(make_diagnostic(
            std::format("wire references unknown source instance '{}'", wire.source_instance),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port));
        return;
    }
    if (target_instance == manifest.instances.end()) {
        diagnostics.push_back(make_diagnostic(
            std::format("wire references unknown target instance '{}'", wire.target_instance),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port));
        return;
    }

    const auto* source_desc = find_descriptor(descriptors, source_instance->component_name);
    const auto* target_desc = find_descriptor(descriptors, target_instance->component_name);

    if (source_desc == nullptr) {
        diagnostics.push_back(make_diagnostic(
            std::format("missing descriptor for source component '{}'",
                        source_instance->component_name),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port));
        return;
    }
    if (target_desc == nullptr) {
        diagnostics.push_back(make_diagnostic(
            std::format("missing descriptor for target component '{}'",
                        target_instance->component_name),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port));
        return;
    }

    const auto* source_port = find_port(*source_desc, wire.source_port);
    const auto* target_port = find_port(*target_desc, wire.target_port);

    if (source_port == nullptr) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "instance '{}' of component '{}' has no declared port '{}'",
                wire.source_instance, source_desc->name, wire.source_port),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port, wire.source_instance,
            source_desc->name));
        return;
    }
    if (target_port == nullptr) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "instance '{}' of component '{}' has no declared port '{}'",
                wire.target_instance, target_desc->name, wire.target_port),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port, wire.target_instance,
            target_desc->name));
        return;
    }

    if (!is_output_direction(source_port->direction)) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "port '{}' on instance '{}' is not an output port",
                wire.source_port, wire.source_instance),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port, wire.source_instance,
            source_desc->name));
    }
    if (!is_input_direction(target_port->direction)) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "port '{}' on instance '{}' is not an input port",
                wire.target_port, wire.target_instance),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port, wire.target_instance,
            target_desc->name));
    }

    const auto incompatibility = contracts_incompatibility(
        *source_port, source_desc, *target_port, target_desc);
    if (incompatibility.has_value()) {
        diagnostics.push_back(make_diagnostic(
            std::format(
                "incompatible contracts on wire {}.{} -> {}.{}: {}",
                wire.source_instance, wire.source_port,
                wire.target_instance, wire.target_port,
                *incompatibility),
            std::nullopt, std::nullopt, wire.source_instance, wire.source_port,
            wire.target_instance, wire.target_port, "", "",
            source_port->contract_name));
    }
}

} // namespace

nexigen::core::Result<std::unordered_map<std::string, nxd::host::ComponentDescriptor>>
load_component_descriptors(const SystemManifest& manifest,
                          const std::filesystem::path& base_path) noexcept {
    std::unordered_map<std::string, nxd::host::ComponentDescriptor> descriptors;
    for (const auto& ref : manifest.components) {
        const auto path = base_path / ref.path;
        const auto loaded = nxd::host::load_component_descriptor(path);
        if (!loaded) {
            return std::unexpected(loaded.error());
        }
        descriptors.emplace(ref.name, *loaded);
    }
    return descriptors;
}

std::vector<ValidationDiagnostic> validate_against_descriptors(
    const SystemManifest& manifest,
    const std::unordered_map<std::string, nxd::host::ComponentDescriptor>&
        descriptors) noexcept {
    std::vector<ValidationDiagnostic> diagnostics;

    for (const auto& instance : manifest.instances) {
        const auto* descriptor = find_descriptor(descriptors, instance.component_name);
        if (descriptor == nullptr) {
            diagnostics.push_back(make_diagnostic(
                std::format("no descriptor loaded for component '{}'",
                            instance.component_name),
                std::nullopt, std::nullopt, "", "", "", "", instance.id,
                instance.component_name));
            continue;
        }
        validate_instance(manifest, instance, *descriptor, diagnostics);
    }

    for (const auto& wire : manifest.wires) {
        validate_wire(manifest, wire, descriptors, diagnostics);
    }

    return diagnostics;
}

} // namespace nxd::runtime::composer
