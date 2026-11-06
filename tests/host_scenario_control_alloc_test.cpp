#include "nxd/host/harness.hpp"
#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <cstdint>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0]
                  << " <scenario_control.descriptor.json> <scenario_control.descriptor.hash>"
                     " <alloc_test.so>\n";
        return 1;
    }

    nxd_blackboard_initialize();

    nxd::host::HeadlessHarness harness(argv[3], argv[1]);
    if (!harness.load()) {
        std::cerr << "failed to load allocation-after-arm test plugin\n";
        return 2;
    }

    if (!harness.initialize("")) {
        std::cerr << "failed to initialize plugin\n";
        return 3;
    }

    if (!harness.arm(1)) {
        std::cerr << "failed to arm plugin\n";
        return 4;
    }

    for (std::uint64_t i = 0; i < 10; ++i) {
        if (!harness.run(1)) {
            std::cerr << "step " << i << " failed or allocation detected after arm\n";
            return 5;
        }
    }

    return 0;
}
