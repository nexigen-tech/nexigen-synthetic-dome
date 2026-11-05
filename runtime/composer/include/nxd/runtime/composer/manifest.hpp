/* NXD Composer — system manifest data model.
 *
 * A system manifest declares the components, instances, partitions, wiring,
 * parameters, thread roles, and Chronos configuration that the Composer turns
 * into a Dome Plan.
 */

#ifndef NXD_RUNTIME_COMPOSER_MANIFEST_HPP
#define NXD_RUNTIME_COMPOSER_MANIFEST_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nxd::runtime::composer {

/// Reference to a component descriptor on disk.
struct ComponentRef final {
    std::string name;
    std::string path;

    [[nodiscard]] bool operator==(const ComponentRef&) const noexcept = default;
};

/// Parameter assignment on an instance.
struct ParameterAssignment final {
    std::string name;
    std::int64_t value = 0;

    [[nodiscard]] bool operator==(const ParameterAssignment&) const noexcept = default;
};

/// Thread-role assignment on an instance.
struct ThreadRoleAssignment final {
    std::string role;
    std::string thread;

    [[nodiscard]] bool operator==(const ThreadRoleAssignment&) const noexcept = default;
};

/// A component instance in the manifest.
struct Instance final {
    std::string id;
    std::string component_name;
    std::uint32_t partition_id = 0;
    std::vector<ParameterAssignment> parameters;
    std::vector<ThreadRoleAssignment> thread_roles;

    [[nodiscard]] bool operator==(const Instance&) const noexcept = default;
};

/// A partition in the manifest.
struct ManifestPartition final {
    std::uint32_t id = 0;
    std::string name;

    [[nodiscard]] bool operator==(const ManifestPartition&) const noexcept = default;
};

/// A wire in the manifest.
struct ManifestWire final {
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    std::optional<std::string> let_boundary_id;

    [[nodiscard]] bool operator==(const ManifestWire&) const noexcept = default;
};

/// A LET boundary in the manifest.
struct ManifestLetBoundary final {
    std::string id;
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    std::int64_t read_time_ns = 0;
    std::int64_t write_deadline_ns = 0;

    [[nodiscard]] bool operator==(const ManifestLetBoundary&) const noexcept = default;
};

/// Chronos configuration in the manifest.
struct ManifestChronosConfig final {
    std::uint8_t mode = 3; // Step
    std::int64_t base_frame_interval_ns = 1'000'000;
    std::uint32_t scale_factor = 1;

    [[nodiscard]] bool operator==(const ManifestChronosConfig&) const noexcept = default;
};

/// A process in the manifest.
struct ManifestProcess final {
    std::string id;
    std::string executable;
    std::vector<std::string> instance_ids;

    [[nodiscard]] bool operator==(const ManifestProcess&) const noexcept = default;
};

/// System manifest consumed by the Composer.
struct SystemManifest final {
    std::vector<ComponentRef> components;
    std::vector<Instance> instances;
    std::vector<ManifestPartition> partitions;
    std::vector<ManifestWire> wires;
    std::vector<ManifestLetBoundary> let_boundaries;
    std::vector<ManifestProcess> processes;
    ManifestChronosConfig chronos;

    /// Parses a system manifest from a JSON string.
    [[nodiscard]] static std::optional<SystemManifest> from_json(std::string_view json);
};

} // namespace nxd::runtime::composer

#endif // NXD_RUNTIME_COMPOSER_MANIFEST_HPP
