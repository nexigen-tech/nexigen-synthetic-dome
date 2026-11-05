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
