/* Deterministic CAN monitor plugin for ABI-1.
 *
 * Reads one RawCanBatchClassical per frame, emits CanAggregateErrors every
 * frame, and emits CanHeartbeat when the configured silence interval has
 * elapsed since the last received frame.
 */

#include "can_monitor_outputs.hpp"

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/sdk.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;

constexpr std::int64_t kDefaultSilenceNs = 2'000'000; // 2 ms

[[nodiscard]] constexpr std::uint64_t fnv1a_hash(std::string_view name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

constexpr std::uint64_t kInputHash = fnv1a_hash("in");
constexpr std::uint64_t kHeartbeatHash = fnv1a_hash("heartbeat");
constexpr std::uint64_t kAggregateHash = fnv1a_hash("aggregate");

struct CanMonitorState {
    bool initialized = false;
    bool armed = false;
    std::array<char, 8> source_name{};
    std::int64_t silence_interval_ns = kDefaultSilenceNs;
    std::int64_t last_seen_ns = 0;
    std::uint32_t heartbeat_counter = 0;
    std::uint32_t last_can_id = 0;
    std::uint8_t last_direction = static_cast<std::uint8_t>(Direction::Rx);
};

CanMonitorState g_state{};

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
    g_state = CanMonitorState{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {
    g_state = CanMonitorState{};
}

NxdStatus initialize(NxdComponentState* state, const char* config, std::uintptr_t config_len) {
    if (state == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    auto* self = reinterpret_cast<CanMonitorState*>(state);
    *self = CanMonitorState{};
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
    auto* self = reinterpret_cast<CanMonitorState*>(state);
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
    auto* self = reinterpret_cast<CanMonitorState*>(state);
    if (!self->armed) {
        return NXD_ERROR_NOT_INITIALIZED;
    }

    const auto* in_slot = find_input(*inputs, kInputHash);
    auto* hb_slot = find_output(*outputs, kHeartbeatHash);
    auto* agg_slot = find_output(*outputs, kAggregateHash);
    if (in_slot == nullptr || hb_slot == nullptr || agg_slot == nullptr) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    if (in_slot->data == nullptr || in_slot->element_size != sizeof(RawCanBatchClassical)) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    const auto* batch = static_cast<const RawCanBatchClassical*>(in_slot->data);

    std::uint32_t rx_valid = 0;
    std::uint32_t rx_invalid = 0;
    std::uint32_t self_rx_count = 0;
    for (std::uint32_t i = 0; i < batch->count; ++i) {
        const auto& frame = batch->frames[i];
        if (frame.kind == Kind::Error) {
            ++rx_invalid;
        } else {
            ++rx_valid;
        }
        if (frame.direction == Direction::SelfRx) {
            ++self_rx_count;
        }
    }
    const std::uint8_t aggregate_status = (rx_invalid > 0) ? 1 /* Degraded */ : 0 /* Valid */;

    can_monitor::fill_aggregate(agg_slot->data,
                                ctx->sim_time_ns,
                                ctx->sim_time_ns,
                                rx_valid,
                                rx_invalid,
                                self_rx_count,
                                static_cast<std::uint32_t>(batch->count),
                                aggregate_status);

    bool emitted = false;
    if (batch->count > 0) {
        const auto& last = batch->frames[batch->count - 1];
        self->last_seen_ns = ctx->sim_time_ns;
        self->last_can_id = last.can_id;
        self->last_direction = static_cast<std::uint8_t>(last.direction);
        can_monitor::fill_heartbeat(hb_slot->data,
                                    last.hw_time_ns,
                                    last.can_id,
                                    self->last_direction,
                                    ++self->heartbeat_counter,
                                    0);
        emitted = true;
    } else if (ctx->sim_time_ns - self->last_seen_ns >= self->silence_interval_ns) {
        can_monitor::fill_heartbeat(hb_slot->data,
                                    ctx->hw_time_ns,
                                    self->last_can_id,
                                    self->last_direction,
                                    ++self->heartbeat_counter,
                                    0);
        emitted = true;
    }

    if (!emitted) {
        // Always leave a well-formed, zeroed heartbeat value.
        std::memset(hb_slot->data, 0, hb_slot->element_size);
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
    const char name[] = "CanMonitor";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash =
        NxdAbiFingerprint{{0xfa, 0x72, 0xf4, 0x80, 0x33, 0x77, 0xc5, 0xa2, 0x88, 0x7e, 0xb0, 0xc0, 0x80, 0x81, 0x16, 0xcc, 0xee, 0x3c, 0xc7, 0x4f, 0xff, 0xc2, 0x5b, 0x26, 0x91, 0xe7, 0xe9, 0x38, 0xa7, 0xe2, 0xe7, 0xa7}};
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
