/* CAN sink for the can_video_demo.
 *
 * Receives RawCanBatchClassical batches via an input port and prints
 * reception statistics.
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/sdk.hpp"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

using nxd::contracts::RawCanBatchClassical;

struct CanSinkState {
    bool initialized = false;
    bool armed = false;
    std::uint64_t total_frames_received = 0;
};

CanSinkState g_state{};

// ── ABI lifecycle ───────────────────────────────────────────────────────────

static NxdComponentState* create() {
    g_state = CanSinkState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

static void destroy(NxdComponentState*) {
    g_state = CanSinkState{};
}

static NxdStatus initialize(NxdComponentState* state, const char*, std::uintptr_t) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSinkState*>(state);
    *self = CanSinkState{};
    self->initialized = true;
    return NXD_OK;
}

static NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSinkState*>(state);
    if (!self->initialized) return NXD_ERROR_NOT_INITIALIZED;
    self->armed = true;
    fprintf(stderr, "[can_sink] armed\n");
    return NXD_OK;
}

static void stop(NxdComponentState*) {}

// ── step_io: read CAN batch ─────────────────────────────────────────────────

static NxdStatus step_io(NxdComponentState* state,
                          const NxdFrameContext* ctx,
                          const NxdPortIoBundle* inputs,
                          NxdPortIoBundle*) {
    if (state == nullptr || ctx == nullptr || inputs == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<CanSinkState*>(state);
    if (!self->armed) return NXD_ERROR_NOT_INITIALIZED;
    if (inputs->count == 0 || inputs->slots == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;

    const nxd::Port<RawCanBatchClassical> in_port(inputs->slots[0]);
    const auto in_span = in_port.as_span();
    if (in_span.empty()) return NXD_OK;

    const RawCanBatchClassical& batch = in_span[0];
    const std::uint32_t count = batch.count;

    fprintf(stderr, "[can_sink]   frame=%04" PRIu64 ": %u CAN frame(s) total=%" PRIu64 "\n",
            ctx->frame_no, count, self->total_frames_received);

    for (std::uint32_t i = 0; i < count && i < 16; ++i) {
        const auto& f = batch.frames[i];
        fprintf(stderr, "[can_sink]     CAN ID=0x%03X dlc=%u [", f.can_id, f.raw_dlc);
        for (std::uint8_t j = 0; j < f.payload_length && j < 8; ++j) {
            if (j > 0) fprintf(stderr, " ");
            fprintf(stderr, "%02X", f.payload[j]);
        }
        fprintf(stderr, "]\n");
    }

    self->total_frames_received += count;
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
    const char name[] = "CanSink";
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