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
        NxdAbiFingerprint{{0x13, 0x41, 0x75, 0x62, 0x2b, 0x0a, 0xda, 0x68, 0x7f, 0xf8, 0x34, 0x7e, 0xbe, 0xc6, 0x3a, 0xe9, 0xe2, 0xa5, 0x09, 0x1b, 0x43, 0xb0, 0x4e, 0x96, 0x4b, 0xc2, 0x5a, 0x1e, 0xea, 0x61, 0x0b, 0x67}};
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.step_io = nullptr;

#ifdef BAD_ABI_VERSION
    table.abi_version = NxdAbiVersion{2, 0, 0};
#endif

#ifdef BAD_TABLE_SIZE
    table.table_size = sizeof(NxdAbiTable) + 64;
#endif

#ifdef BAD_FINGERPRINT
    table.fingerprint = NxdAbiFingerprint{};
#endif

#ifdef BAD_DESCRIPTOR_HASH
    table.descriptor_hash =
        NxdAbiFingerprint{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
#endif

#ifdef BAD_MISSING_FN
    table.step = nullptr;
#endif

    return &table;
}

} // extern "C"
