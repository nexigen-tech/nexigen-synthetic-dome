#include "nxd/abi.hpp"
#include "nxd/sdk.hpp"
#include "nxd/contracts/ScenarioState.hpp"
#include "nxd/contracts/SignalValueU32.hpp"
#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

[[nodiscard]] constexpr std::uint64_t fnv1a_hash(std::string_view name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

constexpr std::uint64_t kOutHash = fnv1a_hash("out");

constexpr std::array<std::uint8_t, 32> kScenarioStateLayoutHash = {
    0x5b, 0x1f, 0xfb, 0xd5, 0x0f, 0x10, 0xca, 0x2b, 0xfe, 0x34, 0xb7, 0xb3, 0x29, 0x85, 0xa1,
    0xeb, 0xf0, 0x7f, 0x70, 0x5a, 0x20, 0x1a, 0x95, 0x3b, 0x1a, 0x5d, 0xca, 0x4c, 0xa6, 0x4b,
    0x84, 0x35};

constexpr std::array<std::uint8_t, 32> kDescriptorHash = {
    0x8c, 0x8b, 0xaf, 0x0c, 0xec, 0xac, 0xa5, 0x91, 0x85, 0x91, 0x11, 0x94, 0xbc, 0x75, 0x55,
    0x7c, 0xcb, 0x21, 0xc2, 0xac, 0x84, 0x03, 0x74, 0xcf, 0x76, 0x6c, 0x28, 0xe7, 0x87, 0x51,
    0x01, 0xa2};

struct ConsumerState {
    const NxdBlackboardEntry* scenario_entry = nullptr;
};

static ConsumerState g_state;

NxdComponentState* create() {
    g_state = ConsumerState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {}

NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) {
    nxd_blackboard_initialize();
    g_state.scenario_entry = nxd_blackboard_open_entry("scenario/state", kScenarioStateLayoutHash.data());
    if (g_state.scenario_entry == nullptr) {
        return NXD_ERROR_NOT_INITIALIZED;
    }
    return NXD_OK;
}

NxdStatus arm(NxdComponentState*, const NxdFrameContext*) {
    return NXD_OK;
}

NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
    return NXD_OK;
}

void stop(NxdComponentState*) {}

NxdStatus step_io(NxdComponentState*,
                  const NxdFrameContext* ctx,
                  const NxdPortIoBundle* inputs,
                  NxdPortIoBundle* outputs) {
    (void)inputs;
    if (ctx == nullptr || outputs == nullptr || outputs->slots == nullptr || outputs->count == 0) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    nxd::contracts::ScenarioState state{};
    std::uint64_t revision = 0;
    std::uint64_t apply_frame_no = 0;
    if (nxd_blackboard_read(g_state.scenario_entry, &state, sizeof(state), &revision, &apply_frame_no) !=
        NXD_BLACKBOARD_OK) {
        return NXD_ERROR_NOT_INITIALIZED;
    }

    for (std::uintptr_t i = 0; i < outputs->count; ++i) {
        NxdPortSlot& slot = outputs->slots[i];
        if (slot.port_name_hash == kOutHash && slot.count > 0 && slot.data != nullptr) {
            auto* out = static_cast<nxd::contracts::SignalValueU32*>(slot.data);
            out->value = state.value;
            out->quality = nxd::contracts::Quality::Valid;
            out->origin = nxd::contracts::Origin::Model;
        }
    }
    return NXD_OK;
}

} // namespace

extern "C" {

const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    table.abi_version = NxdAbiVersion_ABI_1;
    table.table_size = sizeof(NxdAbiTable);
    table.fingerprint = *reinterpret_cast<const NxdAbiFingerprint*>(NXD_ABI_1_TARGET);
    table.sdk_layout_hash = NxdAbiFingerprint{};
    const char name[] = "ScenarioConsumer";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash = *reinterpret_cast<const NxdAbiFingerprint*>(kDescriptorHash.data());
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.step_io = step_io;
    return &table;
}

} // extern "C"
