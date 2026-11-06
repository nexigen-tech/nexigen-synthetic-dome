/* Composer descriptor validation tests.
 *
 * Verifies that the Composer rejects missing required ports, parameters,
 * thread roles, unsupported modes, and incompatible contracts between
 * connected ports.
 */

#include "nxd/runtime/composer/descriptor_validation.hpp"
#include "nxd/runtime/composer/manifest.hpp"
#include "nxd/host/descriptor.hpp"

#include <array>
#include <string>
#include <unordered_map>

namespace composer = nxd::runtime::composer;

[[nodiscard]] nxd::host::ComponentDescriptor make_pid_descriptor() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "pid";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step", "scaled_sil"};

    constexpr std::array<std::uint8_t, 32> setpoint_hash{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

    nxd::host::ContractRef setpoint_contract{};
    setpoint_contract.name = "Setpoint";
    setpoint_contract.version = nxd::host::Version{1, 0, 0};
    setpoint_contract.hash = setpoint_hash;
    desc.contracts.push_back(std::move(setpoint_contract));

    nxd::host::PortDescriptor command{};
    command.name = "command";
    command.direction = nxd::host::PortDescriptor::Direction::Out;
    command.semantics = "Signal";
    command.contract_name = "Setpoint";
    command.contract_version = nxd::host::Version{1, 0, 0};
    command.layout_hash = setpoint_hash;
    command.required = true;
    desc.ports.push_back(std::move(command));

    nxd::host::ParameterDescriptor kp{};
    kp.name = "kp";
    kp.type = "int64";
    kp.min = 0;
    kp.max = 100;
    kp.required = true;
    desc.parameters.push_back(std::move(kp));

    nxd::host::ThreadRoleDescriptor control{};
    control.name = "control";
    control.required = true;
    desc.thread_roles.push_back(std::move(control));

    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_plant_descriptor() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "plant";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step", "realtime_hil"};

    constexpr std::array<std::uint8_t, 32> setpoint_hash{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

    nxd::host::ContractRef setpoint_contract{};
    setpoint_contract.name = "Setpoint";
    setpoint_contract.version = nxd::host::Version{1, 0, 0};
    setpoint_contract.hash = setpoint_hash;
    desc.contracts.push_back(std::move(setpoint_contract));

    nxd::host::PortDescriptor setpoint{};
    setpoint.name = "setpoint";
    setpoint.direction = nxd::host::PortDescriptor::Direction::In;
    setpoint.semantics = "Signal";
    setpoint.contract_name = "Setpoint";
    setpoint.contract_version = nxd::host::Version{1, 0, 0};
    setpoint.layout_hash = setpoint_hash;
    setpoint.required = true;
    desc.ports.push_back(std::move(setpoint));

    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_bad_contract_plant() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "plant_bad";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step"};

    nxd::host::PortDescriptor setpoint{};
    setpoint.name = "setpoint";
    setpoint.direction = nxd::host::PortDescriptor::Direction::In;
    setpoint.semantics = "Signal";
    setpoint.contract_name = "Mismatch";
    setpoint.contract_version = nxd::host::Version{2, 0, 0};
    setpoint.layout_hash = std::array<std::uint8_t, 32>{};
    setpoint.required = true;
    desc.ports.push_back(std::move(setpoint));

    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_bad_hash_plant() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "plant_bad_hash";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step"};

    constexpr std::array<std::uint8_t, 32> bad_hash{
        0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
        0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
        0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
        0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0};

    nxd::host::ContractRef setpoint_contract{};
    setpoint_contract.name = "Setpoint";
    setpoint_contract.version = nxd::host::Version{1, 0, 0};
    setpoint_contract.hash = bad_hash;
    desc.contracts.push_back(std::move(setpoint_contract));

    nxd::host::PortDescriptor setpoint{};
    setpoint.name = "setpoint";
    setpoint.direction = nxd::host::PortDescriptor::Direction::In;
    setpoint.semantics = "Signal";
    setpoint.contract_name = "Setpoint";
    setpoint.contract_version = nxd::host::Version{1, 0, 0};
    setpoint.layout_hash = bad_hash;
    setpoint.required = true;
    desc.ports.push_back(std::move(setpoint));

    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_missing_hash_plant() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "plant_missing_hash";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step"};

    nxd::host::PortDescriptor setpoint{};
    setpoint.name = "setpoint";
    setpoint.direction = nxd::host::PortDescriptor::Direction::In;
    setpoint.semantics = "Signal";
    setpoint.contract_name = "Setpoint";
    setpoint.contract_version = nxd::host::Version{1, 0, 0};
    setpoint.required = true;
    desc.ports.push_back(std::move(setpoint));

    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_hardware_rt_descriptor() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "hardware_rt";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step"};
    desc.resources.requires_hardware_rt = true;
    return desc;
}

[[nodiscard]] nxd::host::ComponentDescriptor make_multi_role_descriptor() {
    nxd::host::ComponentDescriptor desc{};
    desc.name = "multi_role";
    desc.version = nxd::host::Version{1, 0, 0};
    desc.sdk = nxd::host::ComponentDescriptor::SdkVersion{1, 0};
    desc.supported_modes = {"step"};

    nxd::host::ThreadRoleDescriptor control{};
    control.name = "control";
    control.required = true;
    desc.thread_roles.push_back(std::move(control));

    nxd::host::ThreadRoleDescriptor io{};
    io.name = "io";
    io.required = false;
    desc.thread_roles.push_back(std::move(io));

    return desc;
}

[[nodiscard]] composer::SystemManifest make_valid_manifest() {
    composer::SystemManifest m{};
    m.components.push_back(composer::ComponentRef{"pid", "pid.nxdd"});
    m.components.push_back(composer::ComponentRef{"plant", "plant.nxdd"});

    composer::Instance pid{};
    pid.id = "pid_1";
    pid.component_name = "pid";
    pid.partition_id = 1;
    pid.parameters.push_back(composer::ParameterAssignment{"kp", 10});
    pid.thread_roles.push_back(composer::ThreadRoleAssignment{"control", "t0"});
    m.instances.push_back(std::move(pid));

    composer::Instance plant{};
    plant.id = "plant_1";
    plant.component_name = "plant";
    plant.partition_id = 2;
    m.instances.push_back(std::move(plant));

    m.partitions.push_back(composer::ManifestPartition{1, "control"});
    m.partitions.push_back(composer::ManifestPartition{2, "plant"});

    m.let_boundaries.push_back(composer::ManifestLetBoundary{
        .id = "control_to_plant",
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .read_time_ns = 0,
        .write_deadline_ns = 200'000,
    });

    m.wires.push_back(composer::ManifestWire{
        .source_instance = "pid_1",
        .source_port = "command",
        .target_instance = "plant_1",
        .target_port = "setpoint",
        .let_boundary_id = "control_to_plant",
    });

    m.chronos = composer::ManifestChronosConfig{
        .mode = 3, // step
        .base_frame_interval_ns = 1'000'000,
        .scale_factor = 1,
    };

    return m;
}

int main() {
    std::unordered_map<std::string, nxd::host::ComponentDescriptor> descriptors;
    descriptors.emplace("pid", make_pid_descriptor());
    descriptors.emplace("plant", make_plant_descriptor());

    // Valid manifest passes.
    {
        const auto m = make_valid_manifest();
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (!diagnostics.empty()) return 1;
    }

    // Missing required port binding.
    {
        auto m = make_valid_manifest();
        m.wires.clear();
        m.let_boundaries.clear();
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 2) return 10; // pid command + plant setpoint
    }

    // Missing required parameter.
    {
        auto m = make_valid_manifest();
        m.instances[0].parameters.clear();
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 20;
        if (diagnostics[0].component_instance != "pid_1") return 21;
    }

    // Parameter out of range.
    {
        auto m = make_valid_manifest();
        m.instances[0].parameters[0].value = 200;
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 30;
        if (diagnostics[0].message.find("above maximum") == std::string::npos) return 31;
    }

    // Missing required thread role.
    {
        auto m = make_valid_manifest();
        m.instances[0].thread_roles.clear();
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 40;
        if (diagnostics[0].role != "control") return 41;
    }

    // Unsupported chronos mode for pid (pid does not support realtime_hil).
    {
        auto m = make_valid_manifest();
        m.chronos.mode = 0; // realtime_hil
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 50;
        if (diagnostics[0].component_name != "pid") return 51;
    }

    // Wire references unknown instance.
    {
        auto m = make_valid_manifest();
        m.wires[0].target_instance = "missing";
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 2) return 60;
        bool found_unknown_target = false;
        for (const auto& d : diagnostics) {
            if (d.message.find("unknown target instance") != std::string::npos) {
                found_unknown_target = true;
            }
        }
        if (!found_unknown_target) return 61;
    }

    // Incompatible contracts between ports.
    {
        auto bad_descriptors = descriptors;
        bad_descriptors["bad_plant"] = make_bad_contract_plant();

        auto m = make_valid_manifest();
        m.instances[1].component_name = "bad_plant";
        const auto diagnostics = composer::validate_against_descriptors(m, bad_descriptors);
        if (diagnostics.size() != 1) return 70;
        if (diagnostics[0].message.find("incompatible contracts") == std::string::npos) return 71;
    }

    // Incompatible contract layout hash between ports.
    {
        auto bad_descriptors = descriptors;
        bad_descriptors["bad_hash_plant"] = make_bad_hash_plant();

        auto m = make_valid_manifest();
        m.instances[1].component_name = "bad_hash_plant";
        const auto diagnostics = composer::validate_against_descriptors(m, bad_descriptors);
        if (diagnostics.size() != 1) return 80;
        if (diagnostics[0].message.find("layout hash mismatch") == std::string::npos) return 81;
        if (diagnostics[0].message.find("pid_1.command") == std::string::npos) return 82;
        if (diagnostics[0].message.find("plant_1.setpoint") == std::string::npos) return 83;
    }

    // Missing port layout hash.
    {
        auto bad_descriptors = descriptors;
        bad_descriptors["missing_hash_plant"] = make_missing_hash_plant();

        auto m = make_valid_manifest();
        m.instances[1].component_name = "missing_hash_plant";
        const auto diagnostics = composer::validate_against_descriptors(m, bad_descriptors);
        if (diagnostics.size() != 1) return 85;
        if (diagnostics[0].message.find("missing contract layout_hash") == std::string::npos) {
            return 86;
        }
    }

    // Hardware real-time requirement mismatch in non-RT mode.
    {
        auto hw_descriptors = descriptors;
        hw_descriptors["hw"] = make_hardware_rt_descriptor();

        auto m = make_valid_manifest();
        m.instances.push_back(composer::Instance{
            .id = "hw_1",
            .component_name = "hw",
            .partition_id = 1,
            .parameters = {},
            .thread_roles = {},
        });
        const auto diagnostics = composer::validate_against_descriptors(m, hw_descriptors);
        if (diagnostics.size() != 1) return 90;
        if (diagnostics[0].message.find("requires hardware real-time") == std::string::npos) {
            return 91;
        }
    }

    // Undeclared thread role assignment.
    {
        auto m = make_valid_manifest();
        m.instances[0].thread_roles.push_back(
            composer::ThreadRoleAssignment{"missing_role", "t0"});
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 100;
        if (diagnostics[0].message.find("undeclared thread role") == std::string::npos) {
            return 101;
        }
        if (diagnostics[0].role != "missing_role") return 102;
    }

    // Duplicate thread role assignment within the same instance.
    {
        auto m = make_valid_manifest();
        m.instances[0].thread_roles.push_back(
            composer::ThreadRoleAssignment{"control", "t1"});
        const auto diagnostics = composer::validate_against_descriptors(m, descriptors);
        if (diagnostics.size() != 1) return 110;
        if (diagnostics[0].message.find("assigned more than once") == std::string::npos) {
            return 111;
        }
    }

    // Thread role mapped to multiple threads across instances.
    {
        auto mr_descriptors = descriptors;
        mr_descriptors["mr"] = make_multi_role_descriptor();

        auto m = make_valid_manifest();
        m.instances.push_back(composer::Instance{
            .id = "mr_1",
            .component_name = "mr",
            .partition_id = 1,
            .parameters = {},
            .thread_roles = {composer::ThreadRoleAssignment{"control", "t0"}},
        });
        m.instances.push_back(composer::Instance{
            .id = "mr_2",
            .component_name = "mr",
            .partition_id = 2,
            .parameters = {},
            .thread_roles = {composer::ThreadRoleAssignment{"control", "t1"}},
        });
        const auto diagnostics = composer::validate_against_descriptors(m, mr_descriptors);
        if (diagnostics.size() != 1) return 120;
        if (diagnostics[0].message.find("assigned to both") == std::string::npos) {
            return 121;
        }
        if (diagnostics[0].role != "control") return 122;
    }

    return 0;
}
