#pragma once

#include <string>
#include <vector>

#include "nxd/host/descriptor.hpp"
#include <nexigen/core/result.hpp>
#include <nexigen/core/thread_management.hpp>

namespace nxd::host {

    /// Per-role result of validating and optionally applying a component thread
    /// role to the calling thread.
    struct ThreadRoleReport {
        std::string role_name;
        nexigen::core::ThreadSpec spec;
        nexigen::core::ThreadApplyReport apply_report;
    };

    /// Outcome of applying all declared thread roles.  Reports are always returned
    /// so that best-effort partial failures remain visible; `status` carries the
    /// first error when a required role could not be applied.
    struct ThreadRolesOutcome {
        nexigen::core::Status status;
        std::vector<ThreadRoleReport> reports;
    };

    /// Validate that every declared thread role can be represented as a
    /// `core::ThreadSpec`.  Invalid scheduler/affinity/name configurations are
    /// rejected before any operating-system call.
    [[nodiscard]] nexigen::core::Status
    validate_thread_roles(const ComponentDescriptor& desc) noexcept;

    /// Build a `core::ThreadSpec` for every declared role and apply it to the
    /// calling thread.  When `best_effort_only` is true every role is applied with
    /// `BestEffort` semantics so that OS-level rejections are reported but do not
    /// abort arming.  Otherwise roles marked `required` use `Required` semantics
    /// and an OS rejection produces an error status.
    [[nodiscard]] ThreadRolesOutcome apply_thread_roles(const ComponentDescriptor& desc,
                                                        bool best_effort_only = false) noexcept;

} // namespace nxd::host
