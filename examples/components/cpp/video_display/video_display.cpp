/* Video display for the can_video_demo.
 *
 * Receives VideoFrame via iceoryx2 and prints reception metadata.
 */

#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/sdk.hpp"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

using nxd::contracts::VideoFrame;

struct VideoDisplayState {
    bool initialized = false;
    bool armed = false;
    std::uint64_t frames_received = 0;
    std::uint64_t bytes_received = 0;
    std::uint32_t last_seq = 0;
    std::uint32_t gaps_detected = 0;
};

VideoDisplayState g_state{};

static std::uint32_t pixel_checksum(
    const std::array<std::uint8_t, 8388608>& buf,
    std::uint32_t length) {
    std::uint32_t hash = 0x811C9DC5u;
    std::uint32_t end = length < 8388608 ? length : 8388608;
    for (std::uint32_t i = 0; i < end; ++i) {
        hash ^= buf[i];
        hash *= 0x01000193u;
    }
    return hash;
}

static NxdComponentState* create() {
    g_state = VideoDisplayState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

static void destroy(NxdComponentState*) { g_state = VideoDisplayState{}; }

static NxdStatus initialize(NxdComponentState* state, const char*, std::uintptr_t) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoDisplayState*>(state);
    *self = VideoDisplayState{};
    self->initialized = true;
    fprintf(stderr, "[video_display] initialised\n");
    return NXD_OK;
}

static NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoDisplayState*>(state);
    if (!self->initialized) return NXD_ERROR_NOT_INITIALIZED;
    self->armed = true;
    fprintf(stderr, "[video_display] armed\n");
    return NXD_OK;
}

static void stop(NxdComponentState*) {
    fprintf(stderr,
            "[video_display] summary: %" PRIu64 " frames, %" PRIu64 " bytes, %u gaps\n",
            g_state.frames_received, g_state.bytes_received, g_state.gaps_detected);
}

static NxdStatus step_io(NxdComponentState* state,
                          const NxdFrameContext* ctx,
                          const NxdPortIoBundle* inputs,
                          NxdPortIoBundle*) {
    if (state == nullptr || ctx == nullptr || inputs == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoDisplayState*>(state);
    if (!self->armed) return NXD_ERROR_NOT_INITIALIZED;
    if (inputs->count == 0 || inputs->slots == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;

    const nxd::Port<VideoFrame> in_port(inputs->slots[0]);
    const auto in_span = in_port.as_span();
    if (in_span.empty()) return NXD_OK;

    const VideoFrame& frame = in_span[0];

    if (self->frames_received > 0 && frame.content_sequence != self->last_seq + 1) {
        self->gaps_detected++;
        fprintf(stderr, "[video_display] *** GAP: expected seq=%u, got seq=%u\n",
                self->last_seq + 1, frame.content_sequence);
    }
    self->last_seq = frame.content_sequence;

    std::uint32_t cksum = 0;
    if (frame.payload_length > 0)
        cksum = pixel_checksum(frame.payload, frame.payload_length);

    fprintf(stderr,
            "[video_display] frame=%04" PRIu64 ": %ux%u seq=%u/%u payload=%uB cksum=0x%08X\n",
            ctx->frame_no, frame.width, frame.height,
            frame.content_sequence, frame.transport_sequence,
            frame.payload_length, cksum);

    self->frames_received++;
    self->bytes_received += frame.payload_length;
    return NXD_OK;
}

static NxdStatus step(NxdComponentState*, const NxdFrameContext*) { return NXD_OK; }

extern "C" {

const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    table.abi_version = NxdAbiVersion_ABI_1;
    table.table_size = sizeof(NxdAbiTable);
    table.fingerprint = *reinterpret_cast<const NxdAbiFingerprint*>(NXD_ABI_1_TARGET);
    table.sdk_layout_hash = NxdAbiFingerprint{};
    const char name[] = "VideoDisplay";
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