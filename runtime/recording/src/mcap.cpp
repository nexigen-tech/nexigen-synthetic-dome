#include "nxd/runtime/recording/mcap.hpp"

#include "mcap_reader_impl.hpp"
#include "mcap_writer_impl.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace nxd::runtime::recording::mcap {

namespace {

} // namespace

struct Writer::Impl : WriterImpl {};
struct Reader::Impl : ReaderImpl {};

Writer::~Writer() {
    if (impl_ != nullptr) {
        if (!impl_->finalized && impl_->file.is_open()) {
            (void)close();
        }
        delete impl_;
    }
}

bool Writer::open(std::string_view path) noexcept {
    if (impl_ != nullptr) {
        return false;
    }
    impl_ = new Impl();
    impl_->file.open(std::string(path), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!impl_->file.is_open()) {
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    // Magic
    impl_->file.write(reinterpret_cast<const char*>(kMcapMagic.data()), kMcapMagic.size());
    if (!impl_->file.good()) {
        impl_->file.close();
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    // Header record
    const std::string profile = "nexigen";
    const std::string library = "nxd-recording";
    const std::uint64_t header_body_size =
        sizeof(std::uint32_t) + profile.size() +
        sizeof(std::uint32_t) + library.size();
    if (!impl_->write_record_header(kOpHeader, header_body_size) ||
        !impl_->write_string(profile) ||
        !impl_->write_string(library)) {
        impl_->file.close();
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    return true;
}

bool Writer::is_open() const noexcept {
    return impl_ != nullptr && impl_->file.is_open();
}

bool Writer::add_schema(const Schema& schema) noexcept {
    if (impl_ == nullptr || impl_->finalized) return false;
    const std::uint64_t body_size =
        sizeof(std::uint16_t) +                          // id
        sizeof(std::uint32_t) + schema.name.size() +     // name
        sizeof(std::uint32_t) + schema.encoding.size() + // encoding
        sizeof(std::uint64_t) + schema.data.size();      // data
    if (!impl_->write_record_header(kOpSchema, body_size) ||
        !impl_->write_u16(schema.id) ||
        !impl_->write_string(schema.name) ||
        !impl_->write_string(schema.encoding) ||
        !impl_->write_bytes(schema.data.data(), schema.data.size())) {
        return false;
    }
    return true;
}

namespace {

[[nodiscard]] std::uint64_t metadata_body_size(const std::unordered_map<std::string, std::string>& metadata) noexcept {
    std::uint64_t size = sizeof(std::uint32_t); // count
    for (const auto& [key, value] : metadata) {
        size += sizeof(std::uint32_t) + key.size();
        size += sizeof(std::uint32_t) + value.size();
    }
    return size;
}

[[nodiscard]] bool write_metadata(WriterImpl& writer,
                                  const std::unordered_map<std::string, std::string>& metadata) noexcept {
    if (!writer.write_u32(static_cast<std::uint32_t>(metadata.size()))) return false;
    for (const auto& [key, value] : metadata) {
        if (!writer.write_string(key) || !writer.write_string(value)) return false;
    }
    return true;
}

} // namespace

bool Writer::add_channel(const Channel& channel) noexcept {
    if (impl_ == nullptr || impl_->finalized) return false;
    const std::uint64_t meta_size = metadata_body_size(channel.metadata);
    const std::uint64_t body_size =
        sizeof(std::uint16_t) +                                   // id
        sizeof(std::uint16_t) +                                   // schema_id
        sizeof(std::uint32_t) + channel.topic.size() +            // topic
        sizeof(std::uint32_t) + channel.message_encoding.size() + // message_encoding
        meta_size;
    if (!impl_->write_record_header(kOpChannel, body_size) ||
        !impl_->write_u16(channel.id) ||
        !impl_->write_u16(channel.schema_id) ||
        !impl_->write_string(channel.topic) ||
        !impl_->write_string(channel.message_encoding) ||
        !write_metadata(*impl_, channel.metadata)) {
        return false;
    }
    return true;
}

bool Writer::write_message(const Message& message) noexcept {
    if (impl_ == nullptr || impl_->finalized) return false;
    const std::uint64_t body_size =
        sizeof(std::uint16_t) + // channel_id
        sizeof(std::uint32_t) + // sequence
        sizeof(std::uint64_t) + // log_time
        sizeof(std::uint64_t) + // publish_time
        sizeof(std::uint64_t) + // data size
        message.data_size;      // data
    if (!impl_->write_record_header(kOpMessage, body_size) ||
        !impl_->write_u16(message.channel_id) ||
        !impl_->write_u32(message.sequence) ||
        !impl_->write_u64(message.log_time) ||
        !impl_->write_u64(message.publish_time) ||
        !impl_->write_bytes(message.data, message.data_size)) {
        return false;
    }
    ++impl_->message_count;
    impl_->message_start_time = std::min(message.log_time, impl_->message_start_time);
    impl_->message_end_time = std::max(message.log_time, impl_->message_end_time);
    return true;
}

bool Writer::add_attachment(const Attachment& attachment) noexcept {
    if (impl_ == nullptr || impl_->finalized) return false;
    AttachmentEntry entry{};
    entry.name = attachment.name;
    entry.media_type = attachment.media_type;
    entry.log_time = attachment.log_time;
    entry.create_time = attachment.create_time;
    entry.data_size = attachment.data.size();
    entry.file_offset = impl_->current_offset();
    entry.length =
        1 + 8 +                                           // record header
        sizeof(std::uint64_t) +                           // log_time
        sizeof(std::uint64_t) +                           // create_time
        sizeof(std::uint32_t) + entry.name.size() +       // name
        sizeof(std::uint32_t) + entry.media_type.size() + // media_type
        sizeof(std::uint64_t) + entry.data_size +         // data
        sizeof(std::uint32_t);                            // crc
    impl_->attachments.push_back(entry);

    const std::uint64_t body_size =
        sizeof(std::uint64_t) +                           // log_time
        sizeof(std::uint64_t) +                           // create_time
        sizeof(std::uint32_t) + entry.name.size() +       // name
        sizeof(std::uint32_t) + entry.media_type.size() + // media_type
        sizeof(std::uint64_t) + entry.data_size +         // data
        sizeof(std::uint32_t);                            // crc
    if (!impl_->write_record_header(kOpAttachment, body_size) ||
        !impl_->write_u64(attachment.log_time) ||
        !impl_->write_u64(attachment.create_time) ||
        !impl_->write_string(attachment.name) ||
        !impl_->write_string(attachment.media_type) ||
        !impl_->write_bytes(attachment.data.data(), attachment.data.size()) ||
        !impl_->write_u32(attachment.crc)) {
        return false;
    }
    return true;
}

bool Writer::close() noexcept {
    if (impl_ == nullptr) return false;
    if (impl_->finalized || !impl_->file.is_open()) return impl_->finalized;

    if (!impl_->write_data_end()) return false;

    const std::uint64_t summary_start = impl_->current_offset();

    std::uint64_t attachment_index_group_start = 0;
    std::uint64_t attachment_index_group_length = 0;
    if (!impl_->write_attachment_index_group(attachment_index_group_start,
                                             attachment_index_group_length)) {
        return false;
    }

    std::uint64_t stats_group_start = 0;
    std::uint64_t stats_group_length = 0;
    if (!impl_->write_statistics(stats_group_start, stats_group_length)) {
        return false;
    }

    const std::uint64_t summary_offset_start = impl_->current_offset();
    if (!impl_->write_summary_offsets(attachment_index_group_start,
                                      attachment_index_group_length,
                                      stats_group_start,
                                      stats_group_length)) {
        return false;
    }

    return impl_->write_footer_and_magic(summary_start, summary_offset_start);
}

Reader::~Reader() {
    delete impl_;
}

bool Reader::open(std::string_view path) noexcept {
    if (impl_ != nullptr) return false;
    impl_ = new Impl();

    auto fail = [&]() -> bool {
        if (impl_ != nullptr) {
            impl_->file.close();
            delete impl_;
            impl_ = nullptr;
        }
        return false;
    };

    if (!impl_->open_file(path) || !impl_->read_and_validate_magic()) {
        return fail();
    }

    std::uint64_t order = 0;
    bool finished = false;
    while (impl_->file.good()) {
        std::uint8_t opcode = 0;
        std::uint64_t body_size = 0;
        if (!impl_->read_record_header(opcode, body_size)) {
            if (impl_->file.eof()) break;
            return fail();
        }
        const std::uint64_t record_start = impl_->current_offset();

        if (!impl_->dispatch_record(opcode, body_size, order, finished)) {
            return fail();
        }
        if (finished) {
            break;
        }
        if (impl_->is_at_zero_length_record(record_start, body_size)) {
            break;
        }
    }

    impl_->sort_messages();
    impl_->opened_ok = true;
    return true;
}

bool Reader::is_open() const noexcept {
    return impl_ != nullptr && impl_->opened_ok;
}

const std::vector<Schema>& Reader::schemas() const noexcept {
    static const std::vector<Schema> empty;
    return impl_ != nullptr ? impl_->schemas : empty;
}

const std::vector<Channel>& Reader::channels() const noexcept {
    static const std::vector<Channel> empty;
    return impl_ != nullptr ? impl_->channels : empty;
}

const std::vector<Attachment>& Reader::attachments() const noexcept {
    static const std::vector<Attachment> empty;
    return impl_ != nullptr ? impl_->attachments : empty;
}

const std::vector<RecordedMessage>& Reader::messages() const noexcept {
    static const std::vector<RecordedMessage> empty;
    return impl_ != nullptr ? impl_->messages : empty;
}

std::optional<Statistics> Reader::statistics() const noexcept {
    if (impl_ == nullptr) return std::nullopt;
    return impl_->stats;
}

} // namespace nxd::runtime::recording::mcap
