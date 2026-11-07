#pragma once

#include "nxd/runtime/recording/mcap.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nxd::runtime::recording::mcap {

inline constexpr std::array<std::uint8_t, 8> kMcapMagic = {
    0x89, 'M', 'C', 'A', 'P', '0', 0x0D, 0x0A};

inline constexpr std::uint8_t kOpHeader = 0x01;
inline constexpr std::uint8_t kOpFooter = 0x02;
inline constexpr std::uint8_t kOpSchema = 0x03;
inline constexpr std::uint8_t kOpChannel = 0x04;
inline constexpr std::uint8_t kOpMessage = 0x05;
inline constexpr std::uint8_t kOpAttachment = 0x09;
inline constexpr std::uint8_t kOpAttachmentIndex = 0x0A;
inline constexpr std::uint8_t kOpStatistics = 0x0B;
inline constexpr std::uint8_t kOpDataEnd = 0x0F;
inline constexpr std::uint8_t kOpSummaryOffset = 0x0E;

struct ReaderImpl {
    std::ifstream file;
    std::vector<Schema> schemas;
    std::vector<Channel> channels;
    std::vector<Attachment> attachments;
    std::vector<RecordedMessage> messages;
    std::optional<Statistics> stats;
    bool opened_ok = false;

    [[nodiscard]] bool read_u8(std::uint8_t& out) noexcept {
        file.read(reinterpret_cast<char*>(&out), sizeof(out));
        return file.gcount() == sizeof(out);
    }

    [[nodiscard]] bool read_u16(std::uint16_t& out) noexcept {
        file.read(reinterpret_cast<char*>(&out), sizeof(out));
        return file.gcount() == sizeof(out);
    }

    [[nodiscard]] bool read_u32(std::uint32_t& out) noexcept {
        file.read(reinterpret_cast<char*>(&out), sizeof(out));
        return file.gcount() == sizeof(out);
    }

    [[nodiscard]] bool read_u64(std::uint64_t& out) noexcept {
        file.read(reinterpret_cast<char*>(&out), sizeof(out));
        return file.gcount() == sizeof(out);
    }

    [[nodiscard]] bool read_string(std::string& out) noexcept {
        std::uint32_t size = 0;
        if (!read_u32(size)) return false;
        out.resize(size);
        if (size == 0) return true;
        file.read(out.data(), static_cast<std::streamsize>(size));
        return static_cast<std::uint32_t>(file.gcount()) == size;
    }

    [[nodiscard]] bool read_bytes(std::vector<std::byte>& out) noexcept {
        std::uint64_t size = 0;
        if (!read_u64(size)) return false;
        out.resize(size);
        if (size == 0) return true;
        file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
        return static_cast<std::uint64_t>(file.gcount()) == size;
    }

    [[nodiscard]] std::uint64_t current_offset() noexcept {
        return static_cast<std::uint64_t>(file.tellg());
    }

    [[nodiscard]] bool open_file(std::string_view path) noexcept {
        file.open(std::string(path), std::ios::in | std::ios::binary);
        return file.is_open();
    }

    [[nodiscard]] bool read_and_validate_magic() noexcept {
        std::array<std::uint8_t, 8> magic{};
        file.read(reinterpret_cast<char*>(magic.data()), magic.size());
        return file.gcount() == static_cast<std::streamsize>(magic.size()) && magic == kMcapMagic;
    }

    [[nodiscard]] bool read_record_header(std::uint8_t& opcode, std::uint64_t& body_size) noexcept {
        return read_u8(opcode) && read_u64(body_size);
    }

    [[nodiscard]] bool read_header_body(std::uint64_t /*body_size*/) noexcept {
        std::string profile;
        std::string library;
        return read_string(profile) && read_string(library);
    }

    [[nodiscard]] bool read_schema_body(std::uint64_t /*body_size*/) noexcept {
        Schema schema{};
        if (!read_u16(schema.id) || !read_string(schema.name) || !read_string(schema.encoding) ||
            !read_bytes(schema.data)) {
            return false;
        }
        schemas.push_back(std::move(schema));
        return true;
    }

    [[nodiscard]] bool read_channel_body(std::uint64_t /*body_size*/) noexcept {
        Channel channel{};
        std::uint32_t metadata_count = 0;
        if (!read_u16(channel.id) || !read_u16(channel.schema_id) || !read_string(channel.topic) ||
            !read_string(channel.message_encoding) || !read_u32(metadata_count)) {
            return false;
        }
        for (std::uint32_t i = 0; i < metadata_count; ++i) {
            std::string key;
            std::string value;
            if (!read_string(key) || !read_string(value)) {
                return false;
            }
            channel.metadata[key] = std::move(value);
        }
        channels.push_back(std::move(channel));
        return true;
    }

    [[nodiscard]] bool read_message_body(std::uint64_t /*body_size*/, std::uint64_t& order) noexcept {
        RecordedMessage msg{};
        if (!read_u16(msg.channel_id) || !read_u32(msg.sequence) || !read_u64(msg.log_time) ||
            !read_u64(msg.publish_time) || !read_bytes(msg.data)) {
            return false;
        }
        msg.original_order = order++;
        messages.push_back(std::move(msg));
        return true;
    }

    [[nodiscard]] bool read_attachment_body(std::uint64_t /*body_size*/) noexcept {
        Attachment attachment{};
        if (!read_u64(attachment.log_time) || !read_u64(attachment.create_time) ||
            !read_string(attachment.name) || !read_string(attachment.media_type) ||
            !read_bytes(attachment.data) || !read_u32(attachment.crc)) {
            return false;
        }
        attachments.push_back(std::move(attachment));
        return true;
    }

    [[nodiscard]] bool read_statistics_body(std::uint64_t /*body_size*/) noexcept {
        Statistics stats{};
        std::uint32_t channel_counts = 0;
        std::uint32_t metadata_count = 0;
        std::uint32_t chunk_count = 0;
        if (!read_u64(stats.message_count) || !read_u16(stats.schema_count) ||
            !read_u32(stats.channel_count) || !read_u32(stats.attachment_count) ||
            !read_u32(metadata_count) || !read_u32(chunk_count) ||
            !read_u64(stats.message_start_time) || !read_u64(stats.message_end_time) ||
            !read_u32(channel_counts)) {
            return false;
        }
        for (std::uint32_t i = 0; i < channel_counts; ++i) {
            std::uint16_t channel_id = 0;
            std::uint64_t count = 0;
            if (!read_u16(channel_id) || !read_u64(count)) {
                return false;
            }
        }
        this->stats = stats;
        return true;
    }

    [[nodiscard]] bool skip_record_body(std::uint64_t body_size) noexcept {
        if (body_size == 0) return true;
        file.seekg(static_cast<std::streamoff>(body_size), std::ios::cur);
        return file.good();
    }

    [[nodiscard]] bool read_footer_body(std::uint64_t body_size, bool& finished) noexcept {
        if (body_size > 0 && !skip_record_body(body_size)) {
            return false;
        }
        finished = true;
        return true;
    }

    [[nodiscard]] bool dispatch_record(std::uint8_t opcode,
                                       std::uint64_t body_size,
                                       std::uint64_t& order,
                                       bool& finished) noexcept {
        switch (opcode) {
        case kOpHeader:
            return read_header_body(body_size);
        case kOpSchema:
            return read_schema_body(body_size);
        case kOpChannel:
            return read_channel_body(body_size);
        case kOpMessage:
            return read_message_body(body_size, order);
        case kOpAttachment:
            return read_attachment_body(body_size);
        case kOpStatistics:
            return read_statistics_body(body_size);
        case kOpFooter:
            return read_footer_body(body_size, finished);
        default:
            return skip_record_body(body_size);
        }
    }

    [[nodiscard]] bool is_at_zero_length_record(std::uint64_t record_start,
                                                std::uint64_t body_size) noexcept {
        return current_offset() == record_start && body_size == 0 && file.eof();
    }

    void sort_messages() noexcept {
        std::stable_sort(messages.begin(), messages.end(),
                         [](const RecordedMessage& a, const RecordedMessage& b) {
                             return a.log_time < b.log_time;
                         });
    }
};

} // namespace nxd::runtime::recording::mcap
