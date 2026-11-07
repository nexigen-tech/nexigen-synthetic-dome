/* Deterministic video source for the can_video_demo.
 *
 * Generates small RGBA video frames (320x240) at 50 FPS (every 20th tick).
 */

#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/sdk.hpp"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

using nxd::contracts::Codec;
using nxd::contracts::VideoFrame;

constexpr std::uint32_t kDefaultWidth = 320;
constexpr std::uint32_t kDefaultHeight = 240;
constexpr std::uint32_t kFpsNum = 50;
constexpr std::uint32_t kFpsDen = 1;
constexpr std::int64_t kVideoIntervalNs = 20'000'000;
constexpr std::int64_t kFrameNs = 1'000'000;

struct VideoSourceState {
    bool initialized = false;
    bool armed = false;
    std::uint32_t width = kDefaultWidth;
    std::uint32_t height = kDefaultHeight;
    std::uint32_t content_seq = 0;
};

VideoSourceState g_state{};

static std::uint32_t pixel_colour(std::uint32_t x, std::uint32_t y,
                                   std::uint32_t seq, std::uint32_t) {
    std::uint8_t r = static_cast<std::uint8_t>((x ^ y) & 0xFF);
    std::uint8_t g = static_cast<std::uint8_t>((seq * 37 + x / 8) & 0xFF);
    std::uint8_t b = static_cast<std::uint8_t>((y ^ (seq * 13)) & 0xFF);
    return (static_cast<std::uint32_t>(r) << 0) |
           (static_cast<std::uint32_t>(g) << 8) |
           (static_cast<std::uint32_t>(b) << 16) |
           (0xFFu << 24);
}

static NxdComponentState* create() {
    g_state = VideoSourceState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

static void destroy(NxdComponentState*) {
    g_state = VideoSourceState{};
}

static NxdStatus initialize(NxdComponentState* state, const char*, std::uintptr_t) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoSourceState*>(state);
    *self = VideoSourceState{};
    self->initialized = true;
    fprintf(stderr, "[video_source] initialised: %ux%u RGBA @ %u FPS\n",
            self->width, self->height, kFpsNum);
    return NXD_OK;
}

static NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoSourceState*>(state);
    if (!self->initialized) return NXD_ERROR_NOT_INITIALIZED;
    self->armed = true;
    fprintf(stderr, "[video_source] armed\n");
    return NXD_OK;
}

static void stop(NxdComponentState*) {}

static NxdStatus step_io(NxdComponentState* state,
                          const NxdFrameContext* ctx,
                          const NxdPortIoBundle*,
                          NxdPortIoBundle* outputs) {
    if (state == nullptr || ctx == nullptr || outputs == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;
    auto* self = reinterpret_cast<VideoSourceState*>(state);
    if (!self->armed) return NXD_ERROR_NOT_INITIALIZED;
    if (outputs->count == 0 || outputs->slots == nullptr)
        return NXD_ERROR_INVALID_ARGUMENT;

    if (ctx->sim_time_ns % kVideoIntervalNs != 0)
        return NXD_OK;

    NxdPortSlot& slot = outputs->slots[0];
    if (slot.data == nullptr || slot.element_size != sizeof(VideoFrame))
        return NXD_ERROR_INVALID_ARGUMENT;

    auto* frame = static_cast<VideoFrame*>(slot.data);
    std::memset(frame, 0, sizeof(VideoFrame));

    frame->stream_id = 1;
    frame->transport_sequence = self->content_seq;
    frame->content_sequence = self->content_seq;
    frame->width = self->width;
    frame->height = self->height;
    frame->fps_num = kFpsNum;
    frame->fps_den = kFpsDen;
    frame->codec = Codec::Raw;
    frame->plane_offsets[0] = 0;
    frame->plane_strides[0] = self->width * 4;
    frame->payload_length = self->width * self->height * 4;

    if (frame->payload_length > 8388608) {
        fprintf(stderr, "[video_source] payload too large\n");
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    for (std::uint32_t y = 0; y < self->height; ++y) {
        for (std::uint32_t x = 0; x < self->width; ++x) {
            std::uint32_t idx = (y * self->width + x) * 4;
            std::uint32_t colour = pixel_colour(x, y, self->content_seq, self->width);
            frame->payload[idx + 0] = static_cast<std::uint8_t>(colour >> 0);
            frame->payload[idx + 1] = static_cast<std::uint8_t>(colour >> 8);
            frame->payload[idx + 2] = static_cast<std::uint8_t>(colour >> 16);
            frame->payload[idx + 3] = static_cast<std::uint8_t>(colour >> 24);
        }
    }

    fprintf(stderr,
            "[video_source] frame=%04" PRIu64 ": published %ux%u RGBA seq=%u (%u B)\n",
            ctx->frame_no, self->width, self->height,
            self->content_seq, frame->payload_length);

    self->content_seq++;
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
    const char name[] = "VideoSource";
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