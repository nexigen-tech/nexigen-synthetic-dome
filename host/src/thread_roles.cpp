#include "nxd/host/thread_roles.hpp"

#include <string_view>

namespace nxd::host {

namespace {

using nexigen::core::CpuSet;
using nexigen::core::ErrorCode;
using nexigen::core::Result;
using nexigen::core::SchedulingPolicy;
using nexigen::core::Status;
using nexigen::core::ThreadApplyMode;
using nexigen::core::ThreadSpec;

[[nodiscard]] Result<ThreadSpec> build_spec(const ThreadRoleDescriptor& role) noexcept {
    ThreadSpec spec{};

    if (role.single_cpu) {
        spec.set_cpus(CpuSet::from_core_mask(1ULL));
    }

    if (role.default_schedule.has_value()) {
        const std::string_view schedule = *role.default_schedule;
        if (schedule == "other") {
            if (const Status st = spec.set_scheduler(SchedulingPolicy::Other, 0); !st) {
                return std::unexpected(st.error());
            }
        } else if (schedule == "fifo") {
            if (const Status st = spec.set_scheduler(SchedulingPolicy::FIFO, 1); !st) {
                return std::unexpected(st.error());
            }
        } else if (schedule == "rr") {
            if (const Status st = spec.set_scheduler(SchedulingPolicy::RR, 1); !st) {
                return std::unexpected(st.error());
            }
        } else {
            return std::unexpected(ErrorCode::InvalidArgument);
        }
    }

    spec.set_apply_mode(role.required ? ThreadApplyMode::Required
                                      : ThreadApplyMode::BestEffort);

    if (const Status st = spec.set_name(role.name); !st) {
        return std::unexpected(st.error());
    }

    if (const Status st = spec.validate(); !st) {
        return std::unexpected(st.error());
    }

    return spec;
}

} // namespace

Status validate_thread_roles(const ComponentDescriptor& desc) noexcept {
    for (const auto& role : desc.thread_roles) {
        if (const auto spec = build_spec(role); !spec) {
            return std::unexpected(spec.error());
        }
    }
    return {};
}

ThreadRolesOutcome apply_thread_roles(const ComponentDescriptor& desc,
                                      bool best_effort_only) noexcept {
    ThreadRolesOutcome outcome{};
    Status first_apply_error{};

    for (const auto& role : desc.thread_roles) {
        const auto spec_result = build_spec(role);
        if (!spec_result) {
            outcome.status = std::unexpected(spec_result.error());
            return outcome;
        }

        ThreadSpec spec = *spec_result;
        if (best_effort_only) {
            spec.set_apply_mode(ThreadApplyMode::BestEffort);
        }

        ThreadRoleReport report{};
        report.role_name = role.name;
        report.spec = spec;

        const Status applied = nexigen::core::apply_thread_spec_self(spec, report.apply_report);
        outcome.reports.push_back(std::move(report));

        if (!applied && !best_effort_only) {
            outcome.status = applied;
            return outcome;
        }

        if (!applied && !first_apply_error) {
            first_apply_error = applied;
        }
    }

    // Best-effort mode swallows failures so callers can inspect per-role
    // reports; strict mode still clears the status once aggregation is done.
    outcome.status = {};

    return outcome;
}

} // namespace nxd::host
