/* Deterministic video receiver plugin for ABI-1.
 *
 * Reads one VideoFrame per base frame, tracks reception counters, detects gaps,
 * handles VideoHeartbeat NO SIGNAL, and emits VideoAggregateErrors.
 *
 * The skeleton keeps the hot path synchronous and allocation-free.  A separate
 * Qt/OpenGL display tool can consume the same logical source for visual
 * verification on a configured host.
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

constexpr std::int64_t kDefaultSilenceNs = 40'000'000; // 40 ms

[[nodiscard]] constexpr std::uint64_t fnv1a_hash(std::string_view name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

constexpr std::uint64_t kInputHash = fnv1a_hash("in");
constexpr std::uint64_t kAggregateHash = fnv1a_hash("aggregate");

struct VideoOpenglState {
    bool initialized = false;
    bool armed = false;
    std::array<char, 8> source_name{};
    std::int64_t silence_interval_ns = kDefaultSilenceNs;

    std::uint32_t rx_samples = 0;
    std::uint32_t rx_data_frames = 0;
    std::uint32_t displayed_frames = 0;
    std::uint32_t gaps = 0;
    std::uint32_t errors = 0;

    std::uint32_t last_transport_sequence = 0;
    std::uint32_t last_content_sequence = 0;
    bool have_last = false;
    bool no_signal = false;
    std::int64_t last_seen_ns = -kDefaultSilenceNs;
    std::int64_t last_displayed_ns = -kDefaultSilenceNs;
};

VideoOpenglState g_state{};

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

const NxdPortSlot* find_input(const NxdPortIoBundle& bundle, std::uint64_t hash) noexcept {
    for (std::size_t i = 0; i < bundle.count; ++i) {
        if (bundle.slots[i].port_name_hash == hash) {
            return &bundle.slots[i];
        }
    }
    return nullptr;
}

NxdPortSlot* find_output(NxdPortIoBundle& bundle, std::uint64_t hash) noexcept {
    for (std::size_t i = 0; i < bundle.count; ++i) {
        if (bundle.slots[i].port_name_hash == hash) {
            return &bundle.slots[i];
        }
    }
    return nullptr;
}

NxdComponentState* create() {
    g_state = VideoOpenglState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {
    g_state = VideoOpenglState{};
}

NxdStatus initialize(NxdComponentState* state, const char* config, std::uintptr_t config_len) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VideoOpenglState*>(state);
    *self = VideoOpenglState{};
    self->initialized = true;
    set_source_name(self->source_name, config, static_cast<std::size_t>(config_len));
    self->silence_interval_ns = parse_i64(config, static_cast<std::size_t>(config_len),
                                          "silence_interval_ns", kDefaultSilenceNs);
    return NXD_OK;
}

NxdStatus arm(NxdComponentState* state, const NxdFrameContext*) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VideoOpenglState*>(state);
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
    if (state == nullptr || ctx == nullptr || inputs == nullptr || outputs == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<VideoOpenglState*>(state);
    if (!self->armed) {
        return NXD_ERROR_NOT_INITIALIZED;
    }

    const auto* in_slot = find_input(*inputs, kInputHash);
    auto* agg_slot = find_output(*outputs, kAggregateHash);
    if (in_slot == nullptr || agg_slot == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    if (in_slot->data == nullptr || in_slot->element_size != sizeof(VideoFrame)) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    const auto* frame = static_cast<const VideoFrame*>(in_slot->data);

    auto* aggregate = static_cast<VideoAggregateErrors*>(agg_slot->data);
    std::memset(aggregate, 0, sizeof(VideoAggregateErrors));
    aggregate->stream_id = 1;
    aggregate->sequence = self->rx_samples;
    aggregate->status = VideoAggregateStatus::Valid;

    const bool is_heartbeat = frame->codec == Codec::Unknown && frame->payload_length == 0 &&
                              frame->width == 0 && frame->height == 0;

    if (is_heartbeat) {
        // Treat zero-sized unknown codec frames as heartbeat indicators.
        ++self->rx_samples;
        self->last_seen_ns = ctx->sim_time_ns;
        self->no_signal = true;
        aggregate->status = VideoAggregateStatus::MissingLines;
        return NXD_OK;
    }

    if (frame->payload_length == 0) {
        // Empty slot: no sample arrived this step.
        if (ctx->sim_time_ns - self->last_seen_ns >= self->silence_interval_ns) {
            self->no_signal = true;
            aggregate->status = VideoAggregateStatus::MissingLines;
        }
        return NXD_OK;
    }

    ++self->rx_samples;
    ++self->rx_data_frames;
    self->last_seen_ns = ctx->sim_time_ns;
    self->no_signal = false;

    if (self->have_last) {
        const std::uint32_t expected_transport = self->last_transport_sequence + 1;
        if (frame->transport_sequence != expected_transport) {
            const std::uint32_t missing =
                frame->transport_sequence > expected_transport
                    ? frame->transport_sequence - expected_transport
                    : 0;
            self->gaps += missing;
            aggregate->flags |= 1;
            aggregate->status = VideoAggregateStatus::MissingLines;
        }
        const std::uint32_t expected_content = self->last_content_sequence + 1;
        if (frame->content_sequence != expected_content && frame->content_sequence != 0) {
            aggregate->flags |= 2;
            aggregate->status = VideoAggregateStatus::MissingLines;
        }
    }

    self->last_transport_sequence = frame->transport_sequence;
    self->last_content_sequence = frame->content_sequence;
    self->have_last = true;

    if (frame->flags != 0) {
        ++self->errors;
        aggregate->flags |= 4;
        aggregate->status = VideoAggregateStatus::CrcError;
    }

    // Simulate display decimation: "render" one frame every 10 ms.
    if (ctx->sim_time_ns - self->last_displayed_ns >= 10'000'000) {
        ++self->displayed_frames;
        self->last_displayed_ns = ctx->sim_time_ns;
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
    const char name[] = "VideoOpengl";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0x2f, 0x7c, 0xbc, 0x99, 0xb7, 0x3f, 0xfe, 0x16, 0x6a, 0x26, 0xee,
                           0x54, 0x1f, 0xd3, 0x85, 0x9b, 0x94, 0x0e, 0xec, 0xe3, 0x48, 0xfe,
                           0x66, 0xe3, 0x96, 0x57, 0x96, 0x5d, 0x18, 0x6e, 0xdf, 0x52}};
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
