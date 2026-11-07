/* NXD Infrastructure lifecycle test.
 *
 * Verifies that the NXD host policy can initialize and shut down the shared
 * nexigen Infrastructure, that a second instance is rejected, and that the
 * component work path runs between initialization and shutdown.
 */

#include "nxd/host/infrastructure_policy.hpp"
#include <nexigen/app/app_settings.hpp>

#include <cstdlib>
#include <iostream>

namespace {

bool initialize_and_shutdown() {
    auto infra = nexigen::app::make_infrastructure<nxd::host::NxdInfrastructurePolicy>();
    if (!infra) {
        std::cerr << "make_infrastructure failed\n";
        return false;
    }

    nexigen::app::Settings settings{};
    settings.app_name = "nxd-host-infrastructure-test";

    if (const auto status = (*infra)->initialize(settings); !status) {
        std::cerr << "initialize failed\n";
        return false;
    }

    // Component work happens after Infrastructure is ready.
    (*infra)->logger().info("component work before stop");

    if (const auto status = (*infra)->shutdown(); !status) {
        std::cerr << "shutdown failed: "
                  << std::string(nexigen::core::to_string(status.error())) << "\n";
        return false;
    }
    return true;
}

bool second_instance_rejected() {
    auto first = nexigen::app::make_infrastructure<nxd::host::NxdInfrastructurePolicy>();
    if (!first) {
        std::cerr << "first make_infrastructure failed\n";
        return false;
    }

    nexigen::app::Settings settings{};
    settings.app_name = "nxd-host-conflict-test";
    if (const auto status = (*first)->initialize(settings); !status) {
        std::cerr << "first initialize failed\n";
        return false;
    }

    auto second = nexigen::app::make_infrastructure<nxd::host::NxdInfrastructurePolicy>();
    if (!second) {
        std::cerr << "second make_infrastructure failed\n";
        return false;
    }
    const auto status = (*second)->initialize(settings);
    if (status) {
        std::cerr << "second initialize should have failed with Conflict\n";
        return false;
    }
    if (status.error() != nexigen::core::ErrorCode::Conflict) {
        std::cerr << "second initialize returned wrong error code\n";
        return false;
    }

    if (const auto st = (*first)->shutdown(); !st) {
        std::cerr << "first shutdown failed\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!initialize_and_shutdown()) {
        return 1;
    }
    if (!second_instance_rejected()) {
        return 1;
    }
    std::cout << "ok\n";
    return 0;
}
