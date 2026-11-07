#include "nxd/runtime/recording/recorder.hpp"

#include "nxd/contracts/BlackboardUpdate.hpp"
#include "nxd/contracts/RecordingMetadata.hpp"
#include "nxd/runtime/blackboard/blackboard.hpp"
#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/random.h>
#include <unistd.h>
#include <unordered_map>

namespace nxd::runtime::recording {

namespace {

static_assert(sizeof(NxdBlackboardUpdate) == sizeof(nxd::contracts::BlackboardUpdate),
              "BlackboardUpdate C API must match contract layout");

constexpr std::string_view kBlackboardTopic = "_blackboard";
constexpr std::uint16_t kBlackboardSchemaId = 0xFFFFu;
constexpr std::uint16_t kBlackboardChannelId = 0xFFFFu;

void copy_padded_string(std::array<std::uint8_t, 64>& out, std::string_view value) noexcept {
    out = {};
    const std::size_t len = std::min(value.size(), out.size());
    for (std::size_t i = 0; i < len; ++i) {
        out[i] = static_cast<std::uint8_t>(value[i]);
    }
}

[[nodiscard]] bool generate_run_id(std::array<std::uint8_t, 16>& out) noexcept {
    const ssize_t got = getrandom(out.data(), out.size(), 0);
    if (got == static_cast<ssize_t>(out.size())) {
        return true;
    }
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom) {
        urandom.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        if (urandom.gcount() == static_cast<std::streamsize>(out.size())) {
            return true;
        }
    }
    const std::uint64_t now = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t pid = static_cast<std::uint64_t>(getpid());
    std::memcpy(out.data(), &now, sizeof(now));
    std::memcpy(out.data() + sizeof(now), &pid, sizeof(pid));
    return true;
}

} // namespace

struct TopicState {
    transport::Subscriber* subscriber = nullptr;
    std::size_t element_size = 0;
    std::array<std::uint8_t, 32> layout_hash{};
    std::uint32_t last_sequence = 0;
    bool has_recorded = false;
    std::uint16_t channel_id = 0;
    std::uint16_t schema_id = 0;
    std::string schema_name;
    std::uint64_t message_count = 0;
};

struct Recorder::Impl {
    transport::Transport& transport;
    std::string output_path;
    mcap::Writer writer;
    std::unordered_map<std::string, TopicState> topics;
    std::unordered_map<std::string, std::uint64_t> last_blackboard_revision;
    std::uint16_t next_channel_id = 1;
    bool active = false;
    bool blackboard_registered = false;
    std::uint64_t start_sim_time_ns = 0;

    Impl(transport::Transport& t, std::string p) : transport(t), output_path(std::move(p)) {}
};

Recorder::Recorder(transport::Transport& transport, std::string_view output_path) noexcept
    : impl_(new Impl{transport, std::string(output_path)}) {}

Recorder::~Recorder() {
    if (impl_ != nullptr) {
        if (impl_->active) {
            (void)stop();
        }
        delete impl_;
    }
}

bool Recorder::register_topic(std::string_view source_name,
                              std::size_t element_size,
                              std::size_t max_count,
                              const std::array<std::uint8_t, 32>& layout_hash,
                              std::string_view schema_name) noexcept {
    if (impl_ == nullptr || impl_->active) return false;
    const std::string key(source_name);
    if (impl_->topics.contains(key)) {
        return true;
    }
    auto* subscriber = impl_->transport.create_subscriber(source_name, element_size, max_count, layout_hash);
    if (subscriber == nullptr) {
        return false;
    }
    TopicState state{};
    state.subscriber = subscriber;
    state.element_size = element_size;
    state.layout_hash = layout_hash;
    state.channel_id = impl_->next_channel_id;
    state.schema_id = impl_->next_channel_id;
    state.schema_name = schema_name;
    impl_->topics.emplace(key, std::move(state));
    ++impl_->next_channel_id;
    return true;
}

bool Recorder::start() noexcept {
    if (impl_ == nullptr || impl_->active) return false;
    if (!impl_->writer.open(impl_->output_path)) {
        return false;
    }
    impl_->active = true;
    impl_->start_sim_time_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    // Register schemas and channels for each topic.
    for (auto& [name, state] : impl_->topics) {
        mcap::Schema schema{};
        schema.id = state.schema_id;
        schema.name = state.schema_name;
        schema.encoding = "nxd";
        if (!impl_->writer.add_schema(schema)) return false;

        mcap::Channel channel{};
        channel.id = state.channel_id;
        channel.schema_id = state.schema_id;
        channel.topic = name;
        channel.message_encoding = "nxd";
        channel.metadata["element_size"] = std::to_string(state.element_size);
        {
            std::string hash_hex;
            hash_hex.reserve(64);
            static constexpr std::array<char, 16> hex = {
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            for (auto b : state.layout_hash) {
                hash_hex.push_back(hex[(b >> 4) & 0x0F]);
                hash_hex.push_back(hex[b & 0x0F]);
            }
            channel.metadata["layout_hash"] = hash_hex;
        }
        if (!impl_->writer.add_channel(channel)) return false;
    }

    // Register the reserved blackboard channel.
    {
        mcap::Schema bb_schema{};
        bb_schema.id = kBlackboardSchemaId;
        bb_schema.name = "BlackboardUpdate@1.0.0";
        bb_schema.encoding = "nxd";
        if (!impl_->writer.add_schema(bb_schema)) return false;

        mcap::Channel bb_channel{};
        bb_channel.id = kBlackboardChannelId;
        bb_channel.schema_id = kBlackboardSchemaId;
        bb_channel.topic = std::string(kBlackboardTopic);
        bb_channel.message_encoding = "nxd";
        if (!impl_->writer.add_channel(bb_channel)) return false;
        impl_->blackboard_registered = true;
    }
    return true;
}

void Recorder::poll() noexcept {
    if (impl_ == nullptr || !impl_->active) return;
    for (auto& [name, state] : impl_->topics) {
        auto view = state.subscriber->take();
        if (!view.valid() || view.data == nullptr) continue;
        const std::uint32_t seq = view.metadata.sequence;
        if (state.has_recorded && seq == state.last_sequence) continue;
        state.last_sequence = seq;
        state.has_recorded = true;
        const std::size_t payload_size = state.element_size * view.count;
        mcap::Message msg{};
        msg.channel_id = state.channel_id;
        msg.sequence = seq;
        msg.log_time = static_cast<std::uint64_t>(view.metadata.sim_time_ns);
        msg.publish_time = msg.log_time;
        msg.data = view.data;
        msg.data_size = payload_size;
        (void)impl_->writer.write_message(msg);
        ++state.message_count;
    }
}

void Recorder::record_blackboard() noexcept {
    if (impl_ == nullptr || !impl_->active || !impl_->blackboard_registered) return;
    const int count = nxd_blackboard_entry_count();
    for (int i = 0; i < count; ++i) {
        char name_buf[64] = {};
        if (nxd_blackboard_get_entry_name(i, name_buf, sizeof(name_buf)) != NXD_BLACKBOARD_OK) {
            continue;
        }
        const auto* entry = nxd::runtime::blackboard::Blackboard::instance().entry(
            static_cast<std::size_t>(i));
        if (entry == nullptr) continue;
        NxdBlackboardUpdate update{};
        if (entry->get_update(update) != NXD_BLACKBOARD_OK) continue;
        const std::uint64_t revision = update.revision;
        const std::string key(name_buf);
        auto it = impl_->last_blackboard_revision.find(key);
        if (it != impl_->last_blackboard_revision.end() && it->second == revision) {
            continue;
        }
        impl_->last_blackboard_revision[key] = revision;

        mcap::Message msg{};
        msg.channel_id = kBlackboardChannelId;
        msg.sequence = static_cast<std::uint32_t>(revision);
        msg.log_time = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                      std::chrono::steady_clock::now().time_since_epoch())
                                                      .count());
        msg.publish_time = msg.log_time;
        msg.data = reinterpret_cast<const std::byte*>(&update);
        msg.data_size = sizeof(update);
        (void)impl_->writer.write_message(msg);
    }
}

bool Recorder::stop() noexcept {
    if (impl_ == nullptr || !impl_->active) return false;

    nxd::contracts::RecordingMetadata metadata{};
    std::array<std::uint8_t, 16> run_id{};
    (void)generate_run_id(run_id);
    std::memcpy(metadata.run_id.data(), run_id.data(), run_id.size());
    metadata.start_sim_time_ns = static_cast<std::int64_t>(impl_->start_sim_time_ns);
    metadata.channel_count = static_cast<std::uint32_t>(impl_->topics.size());
    std::size_t idx = 0;
    for (const auto& [name, state] : impl_->topics) {
        if (idx >= metadata.channels.size()) break;
        copy_padded_string(metadata.channels[idx].source_name, name);
        copy_padded_string(metadata.channels[idx].schema_name, state.schema_name);
        metadata.channels[idx].message_count = state.message_count;
        ++idx;
    }

    mcap::Attachment attachment{};
    attachment.log_time = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                         std::chrono::steady_clock::now().time_since_epoch())
                                                         .count());
    attachment.create_time = attachment.log_time;
    attachment.name = "RecordingMetadata";
    attachment.media_type = "application/x-nxd.recording-metadata";
    const auto* meta_bytes = reinterpret_cast<const std::byte*>(&metadata);
    attachment.data.assign(meta_bytes, meta_bytes + sizeof(metadata));
    if (!impl_->writer.add_attachment(attachment)) return false;

    const bool ok = impl_->writer.close();
    impl_->active = false;
    return ok;
}

bool Recorder::is_active() const noexcept {
    return impl_ != nullptr && impl_->active;
}

} // namespace nxd::runtime::recording
