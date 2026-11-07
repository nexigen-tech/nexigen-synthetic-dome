/* Deterministic virtual CAN source plugin for ABI-1.
 *
 * Publishes one RawCanBatchClassical each simulation millisecond.  The batch
 * contains all frames whose scheduled send time falls in the current frame
 * interval, sorted by ascending CAN identifier.  Transmitted frames appear as
 * SelfRx loopback entries in the same batch.  Empty batches act as frame-
 * completion watermarks.
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/sdk.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using nxd::contracts::CanFrame;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;
constexpr std::int64_t kFrameNs = 1'000'000; // 1 ms
constexpr std::size_t kScheduleCapacity = 16;
constexpr std::size_t kBatchCapacity = 16; // RawCanBatchClassical::frames

struct ScheduledFrame {
    std::int64_t sim_time_ns = 0;
    std::uint32_t can_id = 0;
    Kind kind = Kind::Data;
    std::uint8_t dlc = 0;
    std::array<std::uint8_t, 8> payload{};
};

struct VirtualCanState {
    bool initialized = false;
    bool armed = false;
    std::array<char, 8> source_name{};
    std::array<ScheduledFrame, kScheduleCapacity> schedule{};
    std::size_t schedule_count = 0;
    std::array<CanFrame, kBatchCapacity> emit_buffer{};
};

VirtualCanState g_state{};

const char* find_value(const char* config, std::size_t len, const char* key) {
    if (config == nullptr || len == 0) {
        return nullptr;
    }
    const std::string_view view(config, len);
    const std::string_view key_quoted = std::string_view(key);
    std::size_t pos = view.find(key_quoted);
    if (pos == std::string_view::npos) {
        return nullptr;
    }
    pos += key_quoted.size();
    while (pos < len && (config[pos] == ' ' || config[pos] == ':' || config[pos] == '"')) {
        ++pos;
    }
    if (pos >= len) {
        return nullptr;
    }
    return config + pos;
}

void set_source_name(VirtualCanState* state, const char* config, std::uintptr_t len) {
    const char* value = find_value(config, static_cast<std::size_t>(len), "source_name");
    if (value != nullptr) {
        std::size_t i = 0;
        while (i < state->source_name.size() && value[i] != '"' && value[i] != '}' && value[i] != ',') {
            state->source_name[i] = value[i];
            ++i;
        }
        while (i < state->source_name.size()) {
            state->source_name[i] = '\0';
            ++i;
        }
    } else {
        const char def[] = "CAN_A";
        for (std::size_t i = 0; i < state->source_name.size(); ++i) {
            state->source_name[i] = (i < sizeof(def) - 1) ? def[i] : '\0';
        }
    }
}

void add_schedule(VirtualCanState* state,
                  std::int64_t sim_time_ns,
                  std::uint32_t can_id,
                  Kind kind,
                  std::uint8_t dlc,
                  std::initializer_list<std::uint8_t> payload) {
    if (state->schedule_count >= state->schedule.size()) {
        return;
    }
    ScheduledFrame frame{};
    frame.sim_time_ns = sim_time_ns;
    frame.can_id = can_id;
    frame.kind = kind;
    frame.dlc = dlc;
    std::size_t i = 0;
    for (std::uint8_t b : payload) {
        if (i >= frame.payload.size()) {
            break;
        }
        frame.payload[i++] = b;
    }
    state->schedule[state->schedule_count++] = frame;
}

NxdComponentState* create() {
    g_state = VirtualCanState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {
    g_state = VirtualCanState{};
}

NxdStatus initialize(NxdComponentState* state, const char* config, std::uintptr_t config_len) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualCanState*>(state);
    *self = VirtualCanState{};
    self->initialized = true;

    set_source_name(self, config, config_len);

    // Deterministic schedule used by the headless tests.  All frames are
    // treated as transmissions by the virtual source and therefore emitted
    // as SelfRx loopback entries.
    add_schedule(self, 0 * kFrameNs, 0x100, Kind::Data, 4, {0x01, 0x02, 0x03, 0x04});
    add_schedule(self, 2 * kFrameNs, 0x100, Kind::Data, 2, {0xAA, 0xBB});
    add_schedule(self, 2 * kFrameNs, 0x080, Kind::Data, 8, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88});
    add_schedule(self, 2 * kFrameNs, 0x701, Kind::Heartbeat, 1, {0x05});
    add_schedule(self, 4 * kFrameNs, 0x200, Kind::Error, 0, {});

    return NXD_OK;
}

NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualCanState*>(state);
    if (!self->initialized) {
        return NXD_ERROR_NOT_INITIALIZED;
    }
    self->armed = true;
    return NXD_OK;
}

void stop(NxdComponentState* /*state*/) {
    // Frame-only teardown: the virtual source keeps its deterministic schedule
    // so that repeated harness runs or stop/arm cycles remain deterministic.
}

NxdStatus step_io(NxdComponentState* state,
                  const NxdFrameContext* ctx,
                  const NxdPortIoBundle* inputs,
                  NxdPortIoBundle* outputs) {
    (void)inputs;
    if (state == nullptr || ctx == nullptr || outputs == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualCanState*>(state);
    if (!self->armed) {
        return NXD_ERROR_NOT_INITIALIZED;
    }
    if (outputs->count == 0 || outputs->slots == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    NxdPortSlot& slot = outputs->slots[0];
    if (slot.data == nullptr || slot.element_size != sizeof(RawCanBatchClassical)) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    auto* batch = static_cast<RawCanBatchClassical*>(slot.data);
    std::memset(batch, 0, sizeof(RawCanBatchClassical));

    const std::int64_t next_sim_time_ns = ctx->sim_time_ns + kFrameNs;
    std::size_t emitted = 0;
    for (std::size_t i = 0; i < self->schedule_count; ++i) {
        const auto& pending = self->schedule[i];
        if (pending.sim_time_ns >= ctx->sim_time_ns && pending.sim_time_ns < next_sim_time_ns) {
            if (emitted >= self->emit_buffer.size()) {
                break;
            }
            CanFrame frame{};
            frame.hw_time_ns = ctx->hw_time_ns + static_cast<std::int64_t>(emitted + 1) * 100;
            frame.can_id = pending.can_id;
            frame.direction = Direction::SelfRx;
            frame.kind = pending.kind;
            frame.raw_dlc = pending.dlc;
            frame.payload_length = pending.dlc;
            frame.payload = pending.payload;
            self->emit_buffer[emitted++] = frame;
        }
    }

    // Stable sort by ascending CAN identifier to model arbitration priority.
    std::sort(self->emit_buffer.begin(), self->emit_buffer.begin() + emitted,
              [](const CanFrame& a, const CanFrame& b) noexcept {
                  return a.can_id < b.can_id;
              });

    for (std::size_t i = 0; i < emitted; ++i) {
        batch->frames[i] = self->emit_buffer[i];
    }
    batch->count = static_cast<std::uint32_t>(emitted);
    batch->assigned_frame = static_cast<std::uint32_t>(ctx->frame_no);

    return NXD_OK;
}

NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
    // The host uses the extended step_io callback when ports are configured.
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
    const char name[] = "VirtualCan";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0xa8, 0x84, 0xd2, 0x3a, 0xc7, 0x95, 0x40, 0xab, 0x39, 0x77, 0x8d, 0x4e, 0xf8, 0xe0, 0xcf, 0xe8, 0xe4, 0xdd, 0x7a, 0x3f, 0x19, 0xc8, 0xe3, 0x20, 0x40, 0xfd, 0xbf, 0xaf, 0xdb, 0x73, 0x73, 0x24}};
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
