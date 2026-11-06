/* Deterministic counter example plugin for ABI-1 (C++).
 *
 * On each frame the component advances an offset using the deterministic RNG
 * exposed through the frame context.  The logic mirrors the Rust example so
 * both plugins produce identical state transitions.
 */

#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstring>

namespace {

    struct CounterState {
        std::uint64_t offset = 0;
        bool armed = false;
    };

    CounterState g_state{};
    std::uint64_t g_initialized = 0;

    NxdComponentState* create() {
        g_state = CounterState{};
        g_initialized = 1;
        return reinterpret_cast<NxdComponentState*>(&g_state);
    }

    void destroy(NxdComponentState*) {
        g_initialized = 0;
    }

    NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) {
        return NXD_OK;
    }

    NxdStatus arm(NxdComponentState* state, const NxdFrameContext* ctx) {
        if (state == nullptr || ctx == nullptr) {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        auto* self = reinterpret_cast<CounterState*>(state);
        nxd::FrameContext frame(*ctx);
        nxd::Rng rng = frame.rng();
        self->offset = rng.next_u64();
        self->armed = true;
        return NXD_OK;
    }

    NxdStatus step(NxdComponentState* state, const NxdFrameContext* ctx) {
        if (state == nullptr || ctx == nullptr) {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        auto* self = reinterpret_cast<CounterState*>(state);
        if (!self->armed) {
            return NXD_ERROR_NOT_INITIALIZED;
        }
        nxd::FrameContext frame(*ctx);
        nxd::Rng rng = frame.rng();
        self->offset += rng.next_u64();
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
