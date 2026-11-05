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
