/* Host component lifecycle and reverse-rollback tests.
 *
 * Uses a test plugin with injected failure stages to verify that failures at
 * initialize, arm, or step prevent later stages and release resources exactly
 * once.
 */

#include "nxd/host/component.hpp"
#include "nxd/host/descriptor.hpp"
#include "nxd/host/plugin.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>

namespace {

using SetFailStageFn = void (*)(std::uint32_t);
using SetFailStatusFn = void (*)(NxdStatus);
using GetCountFn = std::uint32_t (*)();

constexpr std::uint32_t kFailInitialize = 1;
constexpr std::uint32_t kFailArm = 2;
constexpr std::uint32_t kFailStep = 3;

struct TestControl {
    SetFailStageFn set_stage = nullptr;
    SetFailStatusFn set_status = nullptr;
    GetCountFn get_destroy = nullptr;
    GetCountFn get_stop = nullptr;
};

TestControl load_control(void* handle) {
    TestControl ctrl{};
    ctrl.set_stage = reinterpret_cast<SetFailStageFn>(dlsym(handle, "nxd_test_set_fail_stage"));
    ctrl.set_status =
        reinterpret_cast<SetFailStatusFn>(dlsym(handle, "nxd_test_set_fail_status"));
    ctrl.get_destroy =
        reinterpret_cast<GetCountFn>(dlsym(handle, "nxd_test_get_destroy_count"));
    ctrl.get_stop = reinterpret_cast<GetCountFn>(dlsym(handle, "nxd_test_get_stop_count"));
    return ctrl;
}

struct LoadedTest {
    nxd::host::LoadedPlugin plugin;
    TestControl control;
};

LoadedTest load_test(const char* descriptor_path, const char* hash_path,
                     const char* plugin_path) {
    const auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
    const auto hash = nxd::host::load_descriptor_hash(hash_path);
    auto plugin = nxd::host::load_plugin(plugin_path, *descriptor, *hash);
    TestControl ctrl = load_control(plugin->handle);
    return {std::move(*plugin), ctrl};
}

NxdFrameContext make_ctx() {
    NxdFrameContext ctx{};
    ctx.frame_no = 0;
    ctx.sim_time_ns = 0;
    ctx.hw_time_ns = 0;
    ctx.seed_version = 1;
    ctx.rng = nullptr;
    ctx.rng_state = nullptr;
    return ctx;
}

bool success_lifecycle(const char* descriptor_path, const char* hash_path,
                       const char* plugin_path) {
    auto test = load_test(descriptor_path, hash_path, plugin_path);
    nxd::host::ComponentInstance comp(test.plugin);

    test.control.set_stage(0);
    if (const auto st = comp.create(); !st) {
        std::cerr << "success: create failed\n";
        return false;
    }
    if (const auto st = comp.initialize(nullptr, 0); !st) {
        std::cerr << "success: initialize failed\n";
        return false;
    }
    const auto ctx = make_ctx();
    if (const auto st = comp.arm(&ctx); !st) {
        std::cerr << "success: arm failed\n";
        return false;
    }
    for (std::uint64_t frame = 0; frame < 4; ++frame) {
        if (const auto st = comp.step(&ctx); !st) {
            std::cerr << "success: step failed at frame " << frame << "\n";
            return false;
        }
    }
    comp.stop();
    comp.shutdown();

    if (test.control.get_destroy() != 1) {
        std::cerr << "success: expected destroy count 1, got " << test.control.get_destroy()
                  << "\n";
        return false;
    }
    if (test.control.get_stop() != 1) {
        std::cerr << "success: expected stop count 1, got " << test.control.get_stop() << "\n";
        return false;
    }
    nxd::host::unload_plugin(test.plugin);
    return true;
}

bool initialize_failure_rolls_back(const char* descriptor_path, const char* hash_path,
                                   const char* plugin_path) {
    auto test = load_test(descriptor_path, hash_path, plugin_path);
    nxd::host::ComponentInstance comp(test.plugin);

    if (const auto st = comp.create(); !st) {
        std::cerr << "init-fail: create failed\n";
        return false;
    }
    test.control.set_stage(kFailInitialize);
    test.control.set_status(NXD_ERROR_INVALID_ARGUMENT);
    const auto status = comp.initialize(nullptr, 0);
    if (status) {
        std::cerr << "init-fail: initialize should have failed\n";
        return false;
    }
    if (status.error() != nexigen::core::ErrorCode::InvalidArgument) {
        std::cerr << "init-fail: wrong error code\n";
        return false;
    }
    if (comp.last_status() != NXD_ERROR_INVALID_ARGUMENT) {
        std::cerr << "init-fail: last_status not preserved\n";
        return false;
    }

    comp.shutdown();
    if (test.control.get_destroy() != 1) {
        std::cerr << "init-fail: expected destroy count 1, got " << test.control.get_destroy()
                  << "\n";
        return false;
    }
    if (test.control.get_stop() != 0) {
        std::cerr << "init-fail: expected stop count 0, got " << test.control.get_stop()
                  << "\n";
        return false;
    }
    nxd::host::unload_plugin(test.plugin);
    return true;
}

bool arm_failure_rolls_back(const char* descriptor_path, const char* hash_path,
                            const char* plugin_path) {
    auto test = load_test(descriptor_path, hash_path, plugin_path);
    nxd::host::ComponentInstance comp(test.plugin);

    if (const auto st = comp.create(); !st) {
        std::cerr << "arm-fail: create failed\n";
        return false;
    }
    if (const auto st = comp.initialize(nullptr, 0); !st) {
        std::cerr << "arm-fail: initialize failed\n";
        return false;
    }
    test.control.set_stage(kFailArm);
    test.control.set_status(NXD_ERROR_NOT_INITIALIZED);
    const auto ctx = make_ctx();
    const auto status = comp.arm(&ctx);
    if (status) {
        std::cerr << "arm-fail: arm should have failed\n";
        return false;
    }
    if (comp.last_status() != NXD_ERROR_NOT_INITIALIZED) {
        std::cerr << "arm-fail: last_status not preserved\n";
        return false;
    }

    comp.shutdown();
    if (test.control.get_destroy() != 1) {
        std::cerr << "arm-fail: expected destroy count 1, got " << test.control.get_destroy()
                  << "\n";
        return false;
    }
    nxd::host::unload_plugin(test.plugin);
    return true;
}

bool step_failure_rolls_back(const char* descriptor_path, const char* hash_path,
                             const char* plugin_path) {
    auto test = load_test(descriptor_path, hash_path, plugin_path);
    nxd::host::ComponentInstance comp(test.plugin);

    if (const auto st = comp.create(); !st) {
        std::cerr << "step-fail: create failed\n";
        return false;
    }
    if (const auto st = comp.initialize(nullptr, 0); !st) {
        std::cerr << "step-fail: initialize failed\n";
        return false;
    }
    const auto ctx = make_ctx();
    if (const auto st = comp.arm(&ctx); !st) {
        std::cerr << "step-fail: arm failed\n";
        return false;
    }
    test.control.set_stage(kFailStep);
    test.control.set_status(NXD_ERROR_GENERAL);
    const auto status = comp.step(&ctx);
    if (status) {
        std::cerr << "step-fail: step should have failed\n";
        return false;
    }
    if (comp.last_status() != NXD_ERROR_GENERAL) {
        std::cerr << "step-fail: last_status not preserved\n";
        return false;
    }

    comp.shutdown();
    if (test.control.get_destroy() != 1) {
        std::cerr << "step-fail: expected destroy count 1, got " << test.control.get_destroy()
                  << "\n";
        return false;
    }
    if (test.control.get_stop() != 1) {
        std::cerr << "step-fail: expected stop count 1, got " << test.control.get_stop()
                  << "\n";
        return false;
    }
    nxd::host::unload_plugin(test.plugin);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0]
                  << " <descriptor.json> <descriptor.hash> <lifecycle-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* hash_path = argv[2];
    const char* plugin_path = argv[3];

    if (!success_lifecycle(descriptor_path, hash_path, plugin_path)) {
        return 2;
    }
    if (!initialize_failure_rolls_back(descriptor_path, hash_path, plugin_path)) {
        return 3;
    }
    if (!arm_failure_rolls_back(descriptor_path, hash_path, plugin_path)) {
        return 4;
    }
    if (!step_failure_rolls_back(descriptor_path, hash_path, plugin_path)) {
        return 5;
    }

    std::cout << "ok\n";
    return 0;
}
