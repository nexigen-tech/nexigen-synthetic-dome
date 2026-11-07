/* Host thread-role validation and self-application tests.
 *
 * Verifies that declared thread roles are validated before any OS call and that
 * best-effort application keeps partial reports visible.
 */

#include "nxd/host/descriptor.hpp"
#include "nxd/host/thread_roles.hpp"

#include <cstdlib>
#include <iostream>

namespace {

bool deterministic_counter_roles_validate(const char* descriptor_path) {
    const auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
    if (!descriptor) {
        std::cerr << "failed to load descriptor\n";
        return false;
    }
    if (const auto status = nxd::host::validate_thread_roles(*descriptor); !status) {
        std::cerr << "validate_thread_roles failed unexpectedly\n";
        return false;
    }
    return true;
}

bool deterministic_counter_roles_apply_best_effort(const char* descriptor_path) {
    const auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
    if (!descriptor) {
        std::cerr << "failed to load descriptor\n";
        return false;
    }
    const auto outcome = nxd::host::apply_thread_roles(*descriptor, /*best_effort_only=*/true);
    if (!outcome.status) {
        std::cerr << "best-effort apply failed unexpectedly\n";
        return false;
    }
    if (outcome.reports.empty()) {
        std::cerr << "expected at least one thread role report\n";
        return false;
    }
    return true;
}

bool invalid_role_is_rejected() {
    nxd::host::ComponentDescriptor descriptor{};
    descriptor.name = "InvalidThread";
    descriptor.version = {1, 0, 0};
    descriptor.sdk = {1, 0};
    descriptor.resources.deterministic = true;
    descriptor.supported_modes = {"headless"};

    nxd::host::ThreadRoleDescriptor role{};
    role.name = "main";
    role.required = true;
    role.single_cpu = false;
    role.default_schedule = "warp-drive"; // unsupported schedule
    descriptor.thread_roles.push_back(role);

    if (const auto status = nxd::host::validate_thread_roles(descriptor); status) {
        std::cerr << "invalid role should have been rejected\n";
        return false;
    }
    return true;
}

bool best_effort_reports_partial_failure() {
    nxd::host::ComponentDescriptor descriptor{};
    descriptor.name = "PartialThread";
    descriptor.version = {1, 0, 0};
    descriptor.sdk = {1, 0};
    descriptor.resources.deterministic = true;
    descriptor.supported_modes = {"headless"};

    // Request an impossibly high CPU id.  Validation must accept the id
    // (it is within the 0..1023 range), but applying affinity on a typical
    // development host fails and is reported as a best-effort partial failure.
    nxd::host::ThreadRoleDescriptor role{};
    role.name = "main";
    role.required = false;
    role.single_cpu = false;
    descriptor.thread_roles.push_back(role);

    // The descriptor has no explicit requests, so there is nothing to report.
    // Instead add an explicit Other,0 reset so the report is non-trivial.
    nxd::host::ThreadRoleDescriptor reset{};
    reset.name = "control";
    reset.required = false;
    reset.single_cpu = false;
    reset.default_schedule = "other";
    descriptor.thread_roles.push_back(reset);

    const auto outcome = nxd::host::apply_thread_roles(descriptor, /*best_effort_only=*/true);
    if (!outcome.status) {
        std::cerr << "best-effort apply with simple roles failed unexpectedly\n";
        return false;
    }
    if (outcome.reports.size() != 2) {
        std::cerr << "expected two thread role reports\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <descriptor.json>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];

    if (!deterministic_counter_roles_validate(descriptor_path)) {
        return 2;
    }
    if (!deterministic_counter_roles_apply_best_effort(descriptor_path)) {
        return 3;
    }
    if (!invalid_role_is_rejected()) {
        return 4;
    }
    if (!best_effort_reports_partial_failure()) {
        return 5;
    }

    std::cout << "ok\n";
    return 0;
}
