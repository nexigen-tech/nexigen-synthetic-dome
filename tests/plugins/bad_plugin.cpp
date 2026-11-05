/* Shared source for intentionally-invalid ABI-1 plugins.
 *
 * CMake builds multiple shared libraries from this file, each with a different
 * BAD_* define, so host load-plugin tests can verify every rejection path.
 */

#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstring>

namespace {

    struct BadState {};

    BadState g_state{};

    NxdComponentState* create() {
        g_state = BadState{};
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

#if defined(BAD_ABI_VERSION)
    table.abi_version = NxdAbiVersion{2, 0, 0};
#endif

#if defined(BAD_TABLE_SIZE)
    table.table_size = sizeof(NxdAbiTable) + 64;
#endif

#if defined(BAD_FINGERPRINT)
    table.fingerprint = NxdAbiFingerprint{};
#endif

#if defined(BAD_DESCRIPTOR_HASH)
    table.descriptor_hash =
        NxdAbiFingerprint{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
#endif

#if defined(BAD_MISSING_FN)
    table.step = nullptr;
#endif

#if defined(BAD_EXTENSION)
    table.extension = &table;
#endif

    return &table;
}

} // extern "C"
