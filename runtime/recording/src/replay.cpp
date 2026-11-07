#include "nxd/runtime/recording/replay.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>

namespace nxd::runtime::recording {

namespace {

struct ChannelState {
    std::string topic;
    transport::Publisher* publisher = nullptr;
    std::size_t element_size = 0;
    std::array<std::uint8_t, 32> layout_hash{};
};

[[nodiscard]] bool hex_string_to_hash(std::string_view hex,
                                      std::array<std::uint8_t, 32>& out) noexcept {
    out = {};
    if (hex.size() != 64) {
        return false;
    }
    for (std::size_t i = 0; i < 32; ++i) {
        auto nibble = [](char c) -> std::optional<std::uint8_t> {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
            return std::nullopt;
        };
        auto hi = nibble(hex[i * 2]);
        auto lo = nibble(hex[i * 2 + 1]);
        if (!hi || !lo) return false;
        out[i] = static_cast<std::uint8_t>((*hi << 4) | *lo);
    }
    return true;
}

[[nodiscard]] std::int64_t parse_i64(std::string_view s) noexcept {
    std::int64_t value = 0;
    std::size_t i = 0;
    bool neg = false;
    if (!s.empty() && s[0] == '-') {
        neg = true;
        i = 1;
    }
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') break;
        value = value * 10 + static_cast<std::int64_t>(s[i] - '0');
    }
    return neg ? -value : value;
}

} // namespace

struct ReplaySource::Impl {
    transport::Transport& transport;
    std::string mcap_path;
    mcap::Reader reader;
    std::unordered_map<std::uint16_t, ChannelState> channels;
    std::vector<const mcap::RecordedMessage*> messages;
    std::size_t next_index = 0;
    std::uint32_t replay_sequence = 0;
    bool running = false;
    bool paused = false;
    bool loop_enabled = false;
    double speed = 1.0;
    std::int64_t base_sim_time_ns = 0;
    std::int64_t elapsed_offset_ns = 0;
    std::int64_t last_sim_time_ns = 0;
    std::int64_t first_message_time_ns = 0;
    std::int64_t last_message_time_ns = 0;
    bool loaded = false;

    Impl(transport::Transport& t, std::string p) : transport(t), mcap_path(std::move(p)) {}
};

ReplaySource::ReplaySource(transport::Transport& transport, std::string_view mcap_path) noexcept
    : impl_(new Impl(transport, std::string(mcap_path))) {}

ReplaySource::~ReplaySource() {
    delete impl_;
}

bool ReplaySource::load() noexcept {
    if (impl_ == nullptr || impl_->loaded) return false;
    if (!impl_->reader.open(impl_->mcap_path)) {
        return false;
    }
    for (const auto& channel : impl_->reader.channels()) {
        if (channel.topic == "_blackboard") continue;
        ChannelState state{};
        state.topic = channel.topic;
        auto it_size = channel.metadata.find("element_size");
        if (it_size != channel.metadata.end()) {
            state.element_size = static_cast<std::size_t>(parse_i64(it_size->second));
        }
        auto it_hash = channel.metadata.find("layout_hash");
        if (it_hash != channel.metadata.end()) {
            (void)hex_string_to_hash(it_hash->second, state.layout_hash);
        }
        state.publisher = impl_->transport.create_publisher(
            state.topic, state.element_size, 1, state.layout_hash);
        if (state.publisher == nullptr) {
            return false;
        }
        impl_->channels.emplace(channel.id, std::move(state));
    }
    impl_->messages.reserve(impl_->reader.messages().size());
    for (const auto& msg : impl_->reader.messages()) {
        impl_->messages.push_back(&msg);
    }
    if (!impl_->messages.empty()) {
        impl_->first_message_time_ns = static_cast<std::int64_t>(impl_->messages.front()->log_time);
        impl_->last_message_time_ns = static_cast<std::int64_t>(impl_->messages.back()->log_time);
    }
    impl_->loaded = true;
    return true;
}

void ReplaySource::control(const nxd::contracts::ReplayControl& cmd) noexcept {
    if (impl_ == nullptr) return;
    switch (static_cast<int>(cmd.command)) {
    case 1: // Start
        impl_->running = true;
        impl_->paused = false;
        impl_->base_sim_time_ns = 0;
        impl_->elapsed_offset_ns = 0;
        break;
    case 2: // Pause
        if (!impl_->paused && impl_->running) {
            impl_->paused = true;
            if (impl_->base_sim_time_ns != 0) {
                const std::int64_t elapsed_ns = impl_->last_sim_time_ns - impl_->base_sim_time_ns;
                impl_->elapsed_offset_ns +=
                    static_cast<std::int64_t>(static_cast<double>(elapsed_ns) * impl_->speed);
            }
            impl_->base_sim_time_ns = 0;
        }
        break;
    case 3: // Resume
        if (impl_->paused) {
            impl_->paused = false;
            impl_->base_sim_time_ns = impl_->last_sim_time_ns;
        }
        break;
    case 4: // Seek
        if (!impl_->messages.empty()) {
            auto target = cmd.target_sim_time_ns;
            auto it = std::lower_bound(
                impl_->messages.begin(), impl_->messages.end(), target,
                [](const mcap::RecordedMessage* msg, std::int64_t t) {
                    return static_cast<std::int64_t>(msg->log_time) < t;
                });
            impl_->next_index = static_cast<std::size_t>(it - impl_->messages.begin());
            impl_->elapsed_offset_ns = target - impl_->first_message_time_ns;
            impl_->base_sim_time_ns = target;
        }
        break;
    case 5: // SetSpeed
        if (cmd.speed > 0.0) {
            if (impl_->running && !impl_->paused && impl_->base_sim_time_ns != 0) {
                const std::int64_t elapsed_ns =
                    impl_->last_sim_time_ns - impl_->base_sim_time_ns;
                impl_->elapsed_offset_ns +=
                    static_cast<std::int64_t>(static_cast<double>(elapsed_ns) * impl_->speed);
                impl_->base_sim_time_ns = impl_->last_sim_time_ns;
            }
            impl_->speed = cmd.speed;
        }
        break;
    case 6: // SetLoop
        impl_->loop_enabled = cmd.loop_enabled != 0;
        break;
    case 0: // Stop
    default:
        impl_->running = false;
        impl_->paused = false;
        break;
    }
}

bool ReplaySource::step(std::int64_t sim_time_ns) noexcept {
    if (impl_ == nullptr || !impl_->loaded) return false;
    if (!impl_->running || impl_->paused) return true;

    impl_->last_sim_time_ns = sim_time_ns;

    // On the first call after Start or Resume, anchor the timeline.
    if (impl_->base_sim_time_ns == 0) {
        impl_->base_sim_time_ns = sim_time_ns;
    }

    const std::int64_t elapsed_ns = sim_time_ns - impl_->base_sim_time_ns;
    const std::int64_t effective_ns =
        static_cast<std::int64_t>(static_cast<double>(elapsed_ns) * impl_->speed) +
        impl_->elapsed_offset_ns;
    const std::int64_t limit_time = impl_->first_message_time_ns + effective_ns;

    while (impl_->next_index < impl_->messages.size()) {
        const auto* msg = impl_->messages[impl_->next_index];
        if (static_cast<std::int64_t>(msg->log_time) > limit_time) break;

        auto it = impl_->channels.find(msg->channel_id);
        if (it != impl_->channels.end() && it->second.publisher != nullptr) {
            auto loan = it->second.publisher->loan();
            if (loan.valid() && loan.data != nullptr && msg->data.size() <= it->second.element_size) {
                std::memcpy(loan.data, msg->data.data(), msg->data.size());
                transport::SampleMetadata meta{};
                meta.sim_time_ns = static_cast<std::int64_t>(msg->log_time);
                meta.sequence = impl_->replay_sequence++;
                it->second.publisher->publish(meta);
            }
        }
        ++impl_->next_index;
    }

    if (impl_->next_index >= impl_->messages.size()) {
        if (impl_->loop_enabled && !impl_->messages.empty()) {
            impl_->next_index = 0;
            impl_->base_sim_time_ns = sim_time_ns;
            impl_->elapsed_offset_ns = limit_time - impl_->first_message_time_ns;
        } else {
            impl_->running = false;
        }
    }
    return true;
}

bool ReplaySource::is_playing() const noexcept {
    return impl_ != nullptr && impl_->loaded && impl_->running && !impl_->paused;
}

bool ReplaySource::finished() const noexcept {
    if (impl_ == nullptr || !impl_->loaded) return true;
    return !impl_->running && impl_->next_index >= impl_->messages.size();
}

std::int64_t ReplaySource::next_sim_time_ns() const noexcept {
    if (impl_ == nullptr || !impl_->loaded) return -1;
    if (impl_->next_index >= impl_->messages.size()) return -1;
    return static_cast<std::int64_t>(impl_->messages[impl_->next_index]->log_time);
}

} // namespace nxd::runtime::recording
