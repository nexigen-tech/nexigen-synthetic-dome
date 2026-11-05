/* Minimal deterministic ABI-1 plugin with no ports and no runtime allocation.
 *
 * Mirrors the `empty` component descriptor and is used by headless harness
 * tests to measure pure ABI call overhead.
 */

#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstring>

namespace {

    struct EmptyState {};

    EmptyState g_state{};

    NxdComponentState* create() {
        g_state = EmptyState{};
        return reinterpret_cast<NxdComponentState*>(&g_state);
    }

    void destroy(NxdComponentState*) {}

    NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) {
        return NXD_OK;
    }

    NxdStatus arm(NxdComponentState*, const NxdFrameContext*) {
        return NXD_OK;
    }

    NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
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
    const char name[] = "Empty";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0x7f, 0x74, 0xcb, 0x1f, 0xfc, 0xbe, 0x03, 0xed, 0xe2, 0x95, 0xba,
                           0xf6, 0x6d, 0x8e, 0xe0, 0xa5, 0x63, 0xac, 0x28, 0x83, 0xa3, 0x45,
                           0x98, 0x34, 0xfe, 0x70, 0xec, 0xad, 0xb2, 0x91, 0x6f, 0x4e}};
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
