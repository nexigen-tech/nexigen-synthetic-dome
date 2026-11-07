/* Deterministic CAN source for the can_video_demo.
 *
 * Generates a schedule of CAN frames and publishes them as RawCanBatchClassical
 * batches each simulation tick.
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/sdk.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

using nxd::contracts::CanFrame;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;

// ── Schedule ────────────────────────────────────────────────────────────────

struct ScheduledFrame {
    std::int64_t sim_time_ns = 0;
    std::uint32_t can_id = 0;
    Kind kind = Kind::Data;
    std::uint8_t dlc = 0;
    std::array<std::uint8_t, 8> payload{};
};

constexpr std::size_t kScheduleMax = 32;
constexpr std::int64_t kFrameNs = 1'000'000;

struct CanSourceState {
    bool initialized = false;
    bool armed = false;
    std::array<ScheduledFrame, kScheduleMax> schedule{};
    std::size_t schedule_count = 0;
    std::array<CanFrame, 16> emit_buffer{};
};

CanSourceState g_state{};

static void add_schedule(CanSourceState* state,
                          std::int64_t sim_time_ns,
                          std::uint32_t can_id, Kind kind,
                          std::uint8_t dlc,
                          std::initializer_list<std::uint8_t> payload) {
    if (state->schedule_count >= state->schedule.size()) return;
    ScheduledFrame frame{};
    frame.sim_time_ns = sim_time_ns;
    frame.can_id = can_id;
    frame.kind = kind;
    frame.dlc = dlc;
    std::size_t i = 0;
    for (std::uint8_t b : payload) {
        if (i >= frame.payload.size()) break;
        frame.payload[i++] = b;
    }
    state->schedule[state->schedule_count++] = frame;
}

static void build_schedule(CanSourceState* state) {
    add_schedule(state, 0 * kFrameNs, 0x100, Kind::Data, 4, {0xDE, 0xAD, 0xBE, 0xEF});
    add_schedule(state, 0 * kFrameNs, 0x200, Kind::Data, 2, {0xCA, 0xFE});
    add_schedule(state, 0 * kFrameNs, 0x300, Kind::Data, 8,
                 {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    add_schedule(state, 2 * kFrameNs, 0x100, Kind::Data, 1, {0x42});
    add_schedule(state, 2 * kFrameNs, 0x400, Kind::Heartbeat, 0, {});
    add_schedule(state, 4 * kFrameNs, 0x200, Kind::Error, 0, {});
}

// ── ABI lifecycle ───────────────────────────────────────────────────────────

static NxdComponentState* create() {
    g_state = CanSourceState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

static void destroy(NxdComponentState*) {
    g_state = CanSourceState{};
}

static NxdStatus initialize(NxdComponentState* state, const char*, std::uintptr_t) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSourceState*>(state);
    *self = CanSourceState{};
    self->initialized = true;
    build_schedule(self);
    return NXD_OK;
}

static NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSourceState*>(state);
    if (!self->initialized) return NXD_ERROR_NOT_INITIALIZED;
    self->armed = true;
    fprintf(stderr, "[can_source] armed with %zu scheduled frames\n", self->schedule_count);
    return NXD_OK;
}

static void stop(NxdComponentState*) {}

// ── step_io: publish CAN batch ──────────────────────────────────────────────

static NxdStatus step_io(NxdComponentState* state,
                          const NxdFrameContext* ctx,
                          const NxdPortIoBundle*,
                          NxdPortIoBundle* outputs) {
    if (state == nullptr || ctx == nullptr || outputs == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSourceState*>(state);
    if (!self->armed) return NXD_ERROR_NOT_INITIALIZED;
    if (outputs->count == 0 || outputs->slots == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;

    NxdPortSlot& slot = outputs->slots[0];
    if (slot.data == nullptr || slot.element_size != sizeof(RawCanBatchClassical))
        return NXD_ERROR_INVALID_ARGUMENT;

    auto* batch = static_cast<RawCanBatchClassical*>(slot.data);
    std::memset(batch, 0, sizeof(RawCanBatchClassical));

    const std::int64_t next_ns = ctx->sim_time_ns + kFrameNs;
    std::size_t emitted = 0;

    for (std::size_t i = 0; i < self->schedule_count; ++i) {
        const auto& pending = self->schedule[i];
        if (pending.sim_time_ns >= ctx->sim_time_ns && pending.sim_time_ns < next_ns) {
            if (emitted >= self->emit_buffer.size()) break;
            CanFrame f{};
            f.hw_time_ns = ctx->hw_time_ns + static_cast<std::int64_t>(emitted) * 100;
            f.can_id = pending.can_id;
            f.direction = Direction::SelfRx;
            f.kind = pending.kind;
            f.raw_dlc = pending.dlc;
            f.payload_length = pending.dlc;
            f.payload = pending.payload;
            self->emit_buffer[emitted++] = f;
        }
    }

    std::sort(self->emit_buffer.begin(), self->emit_buffer.begin() + emitted,
              [](const CanFrame& a, const CanFrame& b) noexcept {
                  return a.can_id < b.can_id;
              });

    for (std::size_t i = 0; i < emitted; ++i)
        batch->frames[i] = self->emit_buffer[i];

    batch->count = static_cast<std::uint32_t>(emitted);
    batch->assigned_frame = static_cast<std::uint32_t>(ctx->frame_no);

    fprintf(stderr, "[can_source] frame=%04" PRIu64 ": published %zu CAN frame(s)\n",
            ctx->frame_no, emitted);

    return NXD_OK;
}

static NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
    return NXD_OK;
}

extern "C" {

const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    table.abi_version = NxdAbiVersion_ABI_1;
    table.table_size = sizeof(NxdAbiTable);
    table.fingerprint = *reinterpret_cast<const NxdAbiFingerprint*>(NXD_ABI_1_TARGET);
    table.sdk_layout_hash = NxdAbiFingerprint{};
    const char name[] = "CanSource";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash = NxdAbiFingerprint{};
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