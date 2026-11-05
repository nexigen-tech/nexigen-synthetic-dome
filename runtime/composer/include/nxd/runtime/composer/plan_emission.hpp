/* NXD Composer — Dome Plan emission.
 *
 * Transforms a validated system manifest and its loaded component descriptors
 * into a concrete Dome Plan that `nexigen-run` can load and execute without
 * re-parsing the original manifest.
 */

#ifndef NXD_RUNTIME_COMPOSER_PLAN_EMISSION_HPP
#define NXD_RUNTIME_COMPOSER_PLAN_EMISSION_HPP

#include "nxd/host/descriptor.hpp"
#include "nxd/runtime/composer/manifest.hpp"
#include "nxd/runtime/composer/plan.hpp"

#include <string>
#include <unordered_map>

namespace nxd::runtime::composer {

/// Emits a Dome Plan from a manifest and the descriptors it references.
[[nodiscard]] DomePlan emit_plan(
    const SystemManifest& manifest,
    const std::unordered_map<std::string, nxd::host::ComponentDescriptor>& descriptors) noexcept;

} // namespace nxd::runtime::composer

#endif // NXD_RUNTIME_COMPOSER_PLAN_EMISSION_HPP
