/* Test plugin that intentionally allocates during step.
 *
 * Used by headless harness tests to verify that allocation-after-arm is
 * detected and reported without production hot-path instrumentation.
 */

#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

struct AllocatingState {
    bool armed = false;
};

AllocatingState g_state{};

NxdComponentState* create() {
    g_state = AllocatingState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {}

NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) {
    return NXD_OK;
}

NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    g_state.armed = true;
    return NXD_OK;
}

NxdStatus step(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    if (!g_state.armed) {
        return NXD_ERROR_NOT_INITIALIZED;
    }
    // Intentional allocation after arming.  The harness test expects this to
    // be detected and reported as an out-of-memory error.
    void* ptr = std::malloc(64);
    if (ptr != nullptr) {
        std::free(ptr);
        return NXD_ERROR_OUT_OF_MEMORY;
    }
    return NXD_OK;
}

void stop(NxdComponentState*) {}

} // namespace

extern "C" {

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
