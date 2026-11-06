/* Deterministic virtual video source plugin for ABI-1.
 *
 * Publishes one RGBA8888 1920x1080 VideoFrame every 20 ms of simulation time
 * (50 FPS).  Emits VideoHeartbeat during NO SIGNAL intervals and
 * VideoAggregateErrors every base frame.
 */

#include "nxd/contracts/VideoAggregateErrors.hpp"
#include "nxd/contracts/VideoFrame.hpp"
#include "nxd/contracts/VideoHeartbeat.hpp"
#include "nxd/sdk.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using nxd::contracts::Codec;
using nxd::contracts::VideoAggregateErrors;
using nxd::contracts::VideoAggregateStatus;
using nxd::contracts::VideoFrame;
using nxd::contracts::VideoHeartbeat;
using nxd::contracts::VideoHeartbeatStatus;

constexpr std::int64_t kFrameNs = 1'000'000;          // 1 ms base frame
constexpr std::int64_t kVideoFrameNs = 20'000'000;    // 20 ms for 50 FPS
constexpr std::uint32_t kWidth = 1920;
constexpr std::uint32_t kHeight = 1080;
constexpr std::uint32_t kPayloadLength = kWidth * kHeight * 4;

struct VirtualVideoState {
    bool initialized = false;
    bool armed = false;
    std::array<char, 8> source_name{};
    std::int64_t silence_interval_ns = 2 * kVideoFrameNs;
    std::uint32_t gap_every_n_frames = 0;

    std::uint32_t transport_sequence = 0;
    std::uint32_t content_sequence = 0;
    std::uint32_t frames_emitted = 0;
    std::int64_t last_frame_sim_time_ns = 0;
    std::int64_t last_any_sim_time_ns = 0;
    bool no_signal = false;
};

VirtualVideoState g_state{};

std::int64_t parse_i64(const char* config, std::size_t len, const char* key,
                       std::int64_t default_value) noexcept {
    if (config == nullptr || len == 0) {
        return default_value;
    }
    const std::string_view view(config, len);
    const std::string_view key_sv(key);
    std::size_t pos = view.find(key_sv);
    if (pos == std::string_view::npos) {
        return default_value;
    }
    pos += key_sv.size();
    while (pos < len && (config[pos] == ' ' || config[pos] == ':' || config[pos] == '"')) {
        ++pos;
    }
    if (pos >= len) {
        return default_value;
    }
    std::int64_t value = 0;
    std::size_t i = pos;
    while (i < len && config[i] >= '0' && config[i] <= '9') {
        value = value * 10 + static_cast<std::int64_t>(config[i] - '0');
        ++i;
    }
    return value;
}

std::uint32_t parse_u32(const char* config, std::size_t len, const char* key,
                        std::uint32_t default_value) noexcept {
    const std::int64_t value = parse_i64(config, len, key, static_cast<std::int64_t>(default_value));
    if (value < 0) {
        return default_value;
    }
    return static_cast<std::uint32_t>(value);
}

void set_source_name(std::array<char, 8>& out, const char* config, std::size_t len) noexcept {
    out = {};
    if (config == nullptr || len == 0) {
        return;
    }
    const std::string_view view(config, len);
    const std::string_view key = "source_name";
    std::size_t pos = view.find(key);
    if (pos == std::string_view::npos) {
        return;
    }
    pos += key.size();
    while (pos < len && (config[pos] == ' ' || config[pos] == ':' || config[pos] == '"')) {
        ++pos;
    }
    std::size_t i = 0;
    while (i < out.size() && pos < len && config[pos] != '"' && config[pos] != '}' &&
           config[pos] != ',') {
        out[i++] = config[pos++];
    }
}

NxdPortSlot* find_output(NxdPortIoBundle& bundle, std::uint64_t hash) noexcept {
    for (std::size_t i = 0; i < bundle.count; ++i) {
        if (bundle.slots[i].port_name_hash == hash) {
            return &bundle.slots[i];
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr std::uint64_t fnv1a_hash(std::string_view name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

constexpr std::uint64_t kOutHash = fnv1a_hash("out");
constexpr std::uint64_t kHeartbeatHash = fnv1a_hash("heartbeat");
constexpr std::uint64_t kAggregateHash = fnv1a_hash("aggregate");

void generate_pixels(std::uint8_t* pixels, std::uint32_t content_sequence) noexcept {
    // Deterministic rolling gradient plus frame counter overlay.
    const std::uint32_t phase = content_sequence % 256;
    for (std::uint32_t y = 0; y < kHeight; ++y) {
        for (std::uint32_t x = 0; x < kWidth; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * kWidth + x) * 4;
            pixels[idx + 0] = static_cast<std::uint8_t>((x + phase) % 256);     // R
            pixels[idx + 1] = static_cast<std::uint8_t>((y + phase) % 256);     // G
            pixels[idx + 2] = static_cast<std::uint8_t>((x + y + phase) % 256); // B
            pixels[idx + 3] = 0xFF;                                             // A
        }
    }
    // Embed the content sequence in the top-left pixel so tests can read it back.
    pixels[0] = static_cast<std::uint8_t>((content_sequence >> 24) & 0xFF);
    pixels[1] = static_cast<std::uint8_t>((content_sequence >> 16) & 0xFF);
    pixels[2] = static_cast<std::uint8_t>((content_sequence >> 8) & 0xFF);
    pixels[3] = static_cast<std::uint8_t>(content_sequence & 0xFF);
}

NxdComponentState* create() {
    g_state = VirtualVideoState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {
    g_state = VirtualVideoState{};
}

NxdStatus initialize(NxdComponentState* state, const char* config, std::uintptr_t config_len) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualVideoState*>(state);
    *self = VirtualVideoState{};
    self->initialized = true;
    set_source_name(self->source_name, config, static_cast<std::size_t>(config_len));
    self->silence_interval_ns = parse_i64(config, static_cast<std::size_t>(config_len),
                                          "silence_interval_ns", 2 * kVideoFrameNs);
    self->gap_every_n_frames = parse_u32(config, static_cast<std::size_t>(config_len),
                                         "gap_every_n_frames", 0);
    return NXD_OK;
}

NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualVideoState*>(state);
    if (!self->initialized) {
        return NXD_ERROR_NOT_INITIALIZED;
    }
    self->armed = true;
    return NXD_OK;
}

void stop(NxdComponentState* /*state*/) {
    // Frame-only teardown: state is preserved for deterministic replay.
}

NxdStatus step_io(NxdComponentState* state,
                  const NxdFrameContext* ctx,
                  const NxdPortIoBundle* inputs,
                  NxdPortIoBundle* outputs) {
    (void)inputs;
    if (state == nullptr || ctx == nullptr || outputs == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VirtualVideoState*>(state);
    if (!self->armed) {
        return NXD_ERROR_NOT_INITIALIZED;
    }

    auto* out_slot = find_output(*outputs, kOutHash);
    auto* hb_slot = find_output(*outputs, kHeartbeatHash);
    auto* agg_slot = find_output(*outputs, kAggregateHash);
    if (out_slot == nullptr || hb_slot == nullptr || agg_slot == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    if (out_slot->data == nullptr || out_slot->element_size != sizeof(VideoFrame)) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    // Always emit a well-formed aggregate, defaulting to valid.
    auto* aggregate = static_cast<VideoAggregateErrors*>(agg_slot->data);
    std::memset(aggregate, 0, sizeof(VideoAggregateErrors));
    aggregate->stream_id = 1;
    aggregate->sequence = self->transport_sequence;
    aggregate->status = VideoAggregateStatus::Valid;

    // Always leave a well-formed heartbeat; it is overwritten when emitted.
    std::memset(hb_slot->data, 0, hb_slot->element_size);

    const bool due = (ctx->sim_time_ns - self->last_frame_sim_time_ns) >= kVideoFrameNs;
    bool should_drop = false;
    if (due && self->gap_every_n_frames > 0) {
        should_drop = ((self->frames_emitted + 1) % self->gap_every_n_frames) == 0;
    }

    if (due && !should_drop) {
        auto* frame = static_cast<VideoFrame*>(out_slot->data);
        std::memset(frame, 0, sizeof(VideoFrame));
        frame->stream_id = 1;
        frame->transport_sequence = self->transport_sequence++;
        frame->content_sequence = self->content_sequence++;
        frame->width = kWidth;
        frame->height = kHeight;
        frame->fps_num = 50;
        frame->fps_den = 1;
        frame->codec = Codec::Raw;
        frame->plane_offsets[0] = 0;
        frame->plane_strides[0] = kWidth * 4;
        frame->payload_length = kPayloadLength;
        generate_pixels(frame->payload.data(), frame->content_sequence);
        self->last_frame_sim_time_ns = ctx->sim_time_ns;
        self->last_any_sim_time_ns = ctx->sim_time_ns;
        self->frames_emitted++;
        self->no_signal = false;
    } else {
        // No data frame this step: clear the output slot so consumers see absence.
        std::memset(out_slot->data, 0, out_slot->element_size);

        if (due && should_drop) {
            // Injected gap: advance transport sequence but not content sequence.
            aggregate->flags = 1; // Gap injected
            aggregate->status = VideoAggregateStatus::MissingLines;
            self->transport_sequence++;
            self->last_frame_sim_time_ns = ctx->sim_time_ns;
            self->no_signal = false;
        } else if (ctx->sim_time_ns - self->last_any_sim_time_ns >= self->silence_interval_ns) {
            auto* heartbeat = static_cast<VideoHeartbeat*>(hb_slot->data);
            std::memset(heartbeat, 0, sizeof(VideoHeartbeat));
            heartbeat->stream_id = 1;
            heartbeat->transport_sequence = self->transport_sequence++;
            heartbeat->content_sequence = self->content_sequence;
            heartbeat->timestamp_ns = ctx->sim_time_ns;
            heartbeat->status = self->no_signal ? VideoHeartbeatStatus::Lost
                                                : VideoHeartbeatStatus::Stale;
            self->no_signal = true;
        }
    }

    return NXD_OK;
}

NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
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
    const char name[] = "VirtualVideo";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0x51, 0x9f, 0x7f, 0xb4, 0xde, 0xd6, 0x7c, 0x9e, 0x97, 0xc3, 0x1e, 0xc1, 0xf6, 0x8c, 0x01, 0x63, 0x7d, 0x2d, 0xbf, 0x52, 0x88, 0x77, 0x18, 0x5a, 0x4c, 0x71, 0x6e, 0xba, 0xd3, 0x3d, 0x28, 0x5c}};
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
