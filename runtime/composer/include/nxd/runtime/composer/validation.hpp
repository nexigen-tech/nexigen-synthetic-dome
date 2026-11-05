/* NXD Composer — validation rules.
 *
 * Validates a Dome Plan against composition rules, producing human-readable
 * diagnostics for every violation.
 */

#ifndef NXD_RUNTIME_COMPOSER_VALIDATION_HPP
#define NXD_RUNTIME_COMPOSER_VALIDATION_HPP

#include "nxd/runtime/composer/plan.hpp"
#include <optional>
#include <string>
#include <vector>

namespace nxd::runtime::composer {

/// A single validation diagnostic.
struct ValidationDiagnostic final {
    std::string message;
    std::optional<std::uint32_t> source_partition_id;
    std::optional<std::uint32_t> target_partition_id;
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    std::string component_instance;
    std::string component_name;
    std::string contract_name;
    std::string role;

    [[nodiscard]] bool operator==(const ValidationDiagnostic&) const noexcept = default;
};

/// Validates that every wire without an explicit LET boundary stays inside a
/// single partition.  Returns one diagnostic per invalid wire; an empty
/// result means the plan passes this rule.
[[nodiscard]] std::vector<ValidationDiagnostic>
validate_intra_partition_direct_wiring(const DomePlan& plan) noexcept;

/// Validates that every cross-partition wire references an existing LET
/// boundary whose source/target instance and port declarations match the
/// wire.  Returns one diagnostic per invalid wire.
[[nodiscard]] std::vector<ValidationDiagnostic>
validate_let_boundaries(const DomePlan& plan) noexcept;

/// Validates that wires never connect instances belonging to different
/// logical source names.  A partition without an explicit source name is
/// treated as compatible with any source, preserving legacy plans.
/// Returns one diagnostic per cross-source wire.
[[nodiscard]] std::vector<ValidationDiagnostic>
validate_source_name_wiring(const DomePlan& plan) noexcept;

} // namespace nxd::runtime::composer

#endif // NXD_RUNTIME_COMPOSER_VALIDATION_HPP
