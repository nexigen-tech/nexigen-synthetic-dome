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
        NxdAbiFingerprint{{0x13, 0x41, 0x75, 0x62, 0x2b, 0x0a, 0xda, 0x68, 0x7f, 0xf8, 0x34, 0x7e, 0xbe, 0xc6, 0x3a, 0xe9, 0xe2, 0xa5, 0x09, 0x1b, 0x43, 0xb0, 0x4e, 0x96, 0x4b, 0xc2, 0x5a, 0x1e, 0xea, 0x61, 0x0b, 0x67}};
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.step_io = nullptr;
    return &table;
}

} // extern "C"
