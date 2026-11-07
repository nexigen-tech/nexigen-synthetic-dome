#pragma once

#include "mcap_reader_impl.hpp"
#include "nxd/runtime/recording/mcap.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace nxd::runtime::recording::mcap {

struct AttachmentEntry {
    std::string name;
    std::string media_type;
    std::uint64_t log_time = 0;
    std::uint64_t create_time = 0;
    std::uint64_t data_size = 0;
    std::uint64_t file_offset = 0;
    std::uint64_t length = 0;
};

struct WriterImpl {
    std::ofstream file;
    std::vector<AttachmentEntry> attachments;
    std::uint64_t message_count = 0;
    std::uint64_t message_start_time = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t message_end_time = 0;
    bool finalized = false;

    [[nodiscard]] bool write_u8(std::uint8_t value) noexcept {
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return file.good();
    }

    [[nodiscard]] bool write_u16(std::uint16_t value) noexcept {
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return file.good();
    }

    [[nodiscard]] bool write_u32(std::uint32_t value) noexcept {
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return file.good();
    }

    [[nodiscard]] bool write_u64(std::uint64_t value) noexcept {
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return file.good();
    }

    [[nodiscard]] bool write_string(std::string_view value) noexcept {
        const auto size = static_cast<std::uint32_t>(value.size());
        if (!write_u32(size)) return false;
        if (size == 0) return true;
        file.write(value.data(), static_cast<std::streamsize>(size)); // NOLINT(bugprone-suspicious-stringview-data-usage)
        return file.good();
    }

    [[nodiscard]] bool write_bytes(const std::byte* data, std::size_t size) noexcept {
        if (!write_u64(static_cast<std::uint64_t>(size))) return false;
        if (size == 0) return true;
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.good();
    }

    [[nodiscard]] bool write_record_header(std::uint8_t opcode, std::uint64_t body_size) noexcept {
        return write_u8(opcode) && write_u64(body_size);
    }

    [[nodiscard]] std::uint64_t current_offset() noexcept {
        return static_cast<std::uint64_t>(file.tellp());
    }

    [[nodiscard]] bool write_data_end() noexcept {
        const std::uint64_t data_end_size = sizeof(std::uint32_t); // data_section_crc = 0
        return write_record_header(kOpDataEnd, data_end_size) && write_u32(0u);
    }

    [[nodiscard]] bool write_attachment_index_group(std::uint64_t& out_start,
                                                    std::uint64_t& out_length) noexcept {
        out_start = current_offset();
        out_length = 0;
        for (const auto& entry : attachments) {
            const std::uint64_t index_length =
                sizeof(std::uint64_t) + // offset
                sizeof(std::uint64_t) + // length
                sizeof(std::uint64_t) + // log_time
                sizeof(std::uint64_t) + // create_time
                sizeof(std::uint64_t) + // data_size
                sizeof(std::uint32_t) + entry.name.size() +
                sizeof(std::uint32_t) + entry.media_type.size();
            if (!write_record_header(kOpAttachmentIndex, index_length) ||
                !write_u64(entry.file_offset) ||
                !write_u64(entry.length) ||
                !write_u64(entry.log_time) ||
                !write_u64(entry.create_time) ||
                !write_u64(entry.data_size) ||
                !write_string(entry.name) ||
                !write_string(entry.media_type)) {
                return false;
            }
            out_length += index_length + 1 + 8;
        }
        return true;
    }

    [[nodiscard]] bool write_statistics(std::uint64_t& out_group_start,
                                        std::uint64_t& out_group_length) noexcept {
        Statistics stats{};
        stats.message_count = message_count;
        stats.schema_count = 0;  // not tracked per file
        stats.channel_count = 0; // not tracked per file
        stats.attachment_count = static_cast<std::uint32_t>(attachments.size());
        stats.message_start_time = message_count == 0 ? 0 : message_start_time;
        stats.message_end_time = message_end_time;

        out_group_start = current_offset();
        const std::uint64_t stats_body_size =
            sizeof(std::uint64_t) + // message_count
            sizeof(std::uint16_t) + // schema_count
            sizeof(std::uint32_t) + // channel_count
            sizeof(std::uint32_t) + // attachment_count
            sizeof(std::uint32_t) + // metadata_count
            sizeof(std::uint32_t) + // chunk_count
            sizeof(std::uint64_t) + // message_start_time
            sizeof(std::uint64_t) + // message_end_time
            sizeof(std::uint32_t);  // channel_message_counts count = 0
        out_group_length = stats_body_size + 1 + 8;

        return write_record_header(kOpStatistics, stats_body_size) &&
               write_u64(stats.message_count) &&
               write_u16(stats.schema_count) &&
               write_u32(stats.channel_count) &&
               write_u32(stats.attachment_count) &&
               write_u32(0u) && // metadata_count
               write_u32(0u) && // chunk_count
               write_u64(stats.message_start_time) &&
               write_u64(stats.message_end_time) &&
               write_u32(0u); // channel_message_counts count
    }

    [[nodiscard]] bool write_summary_offsets(std::uint64_t attachment_index_group_start,
                                             std::uint64_t attachment_index_group_length,
                                             std::uint64_t stats_group_start,
                                             std::uint64_t stats_group_length) noexcept {
        const std::uint64_t body_size =
            sizeof(std::uint8_t) +
            sizeof(std::uint64_t) +
            sizeof(std::uint64_t);

        if (!attachments.empty()) {
            if (!write_record_header(kOpSummaryOffset, body_size) ||
                !write_u8(kOpAttachmentIndex) ||
                !write_u64(attachment_index_group_start) ||
                !write_u64(attachment_index_group_length)) {
                return false;
            }
        }

        return write_record_header(kOpSummaryOffset, body_size) &&
               write_u8(kOpStatistics) &&
               write_u64(stats_group_start) &&
               write_u64(stats_group_length);
    }

    [[nodiscard]] bool write_footer_and_magic(std::uint64_t summary_start,
                                              std::uint64_t summary_offset_start) noexcept {
        const std::uint64_t footer_body_size =
            sizeof(std::uint64_t) + // summaryStart
            sizeof(std::uint64_t) + // summaryOffsetStart
            sizeof(std::uint32_t);  // summaryCrc

        const bool ok =
            write_record_header(kOpFooter, footer_body_size) &&
            write_u64(summary_start) &&
            write_u64(summary_offset_start) &&
            write_u32(0u);
        if (!ok) return false;

        file.write(reinterpret_cast<const char*>(kMcapMagic.data()), kMcapMagic.size());
        file.flush();
        finalized = true;
        return file.good();
    }
};

} // namespace nxd::runtime::recording::mcap
