/* NXD Composer — component-descriptor validation.
 *
 * Validates a system manifest against the component descriptors it references,
 * including required ports, parameters, thread roles, supported modes,
 * contract identities, and layout hashes.
 */

#ifndef NXD_RUNTIME_COMPOSER_DESCRIPTOR_VALIDATION_HPP
#define NXD_RUNTIME_COMPOSER_DESCRIPTOR_VALIDATION_HPP

#include "nxd/runtime/composer/manifest.hpp"
#include "nxd/runtime/composer/validation.hpp"
#include "nxd/host/descriptor.hpp"
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace nxd::runtime::composer {

/// Loads the component descriptors referenced by a manifest.  Paths are
/// resolved relative to `base_path`.
[[nodiscard]] nexigen::core::Result<
    std::unordered_map<std::string, nxd::host::ComponentDescriptor>>
load_component_descriptors(const SystemManifest& manifest,
                          const std::filesystem::path& base_path) noexcept;

/// Validates the manifest against the supplied component descriptors.
[[nodiscard]] std::vector<ValidationDiagnostic>
validate_against_descriptors(
    const SystemManifest& manifest,
    const std::unordered_map<std::string, nxd::host::ComponentDescriptor>&
        descriptors) noexcept;

} // namespace nxd::runtime::composer

#endif // NXD_RUNTIME_COMPOSER_DESCRIPTOR_VALIDATION_HPP
