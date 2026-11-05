/* Test plugin for host lifecycle rollback verification.
 *
 * The plugin exposes global control symbols so a test can ask it to fail at
 * initialize, arm, or step.  It also counts destroy/stop calls so the test can
 * verify reverse rollback happened exactly once.
 */

#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstring>

namespace {

    struct LifecycleState {
        std::uint32_t fail_stage = 0;
        NxdStatus fail_status = NXD_ERROR_GENERAL;
    };

    LifecycleState g_state{};
    std::uint32_t g_destroy_count = 0;
    std::uint32_t g_stop_count = 0;

    NxdComponentState* create() {
        g_state = LifecycleState{};
        return reinterpret_cast<NxdComponentState*>(&g_state);
    }

    void destroy(NxdComponentState*) {
        ++g_destroy_count;
    }

    NxdStatus initialize(NxdComponentState* state, const char*, std::uintptr_t) {
        if (state == nullptr) {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        if (g_state.fail_stage == 1) {
            return g_state.fail_status;
        }
        return NXD_OK;
    }

    NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
        if (state == nullptr) {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        if (g_state.fail_stage == 2) {
            return g_state.fail_status;
        }
        return NXD_OK;
    }

    NxdStatus step(NxdComponentState* state, const NxdFrameContext*) {
        if (state == nullptr) {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        if (g_state.fail_stage == 3) {
            return g_state.fail_status;
        }
        return NXD_OK;
    }

    void stop(NxdComponentState*) {
        ++g_stop_count;
    }

} // namespace

extern "C" {

void nxd_test_set_fail_stage(std::uint32_t stage) {
    g_state.fail_stage = stage;
}
void nxd_test_set_fail_status(NxdStatus status) {
    g_state.fail_status = status;
}
std::uint32_t nxd_test_get_destroy_count() {
    return g_destroy_count;
}
std::uint32_t nxd_test_get_stop_count() {
    return g_stop_count;
}

const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    table.abi_version = NxdAbiVersion_ABI_1;
    table.table_size = sizeof(NxdAbiTable);
    table.fingerprint = *reinterpret_cast<const NxdAbiFingerprint*>(NXD_ABI_1_TARGET);
    table.sdk_layout_hash = NxdAbiFingerprint{};
    const char name[] = "DeterministicCounter";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0x01, 0x13, 0xf6, 0x17, 0xb2, 0xa2, 0x1f, 0x87, 0x92, 0xd7, 0x7a,
                           0xfa, 0x9f, 0xea, 0x3c, 0x84, 0xee, 0x4c, 0x35, 0x90, 0x9c, 0x7b,
                           0xec, 0x57, 0xa3, 0xd8, 0x2a, 0xa4, 0xf2, 0x1b, 0x45, 0x99}};
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.extension = nullptr;
    return &table;
}

} // extern "C"
