#include "mcap_reader_impl.hpp"
#include "mcap_writer_impl.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace {

using namespace nxd::runtime::recording::mcap;

constexpr std::uint64_t kRecordHeaderSize = 1 + 8;

std::filesystem::path make_temp_path() {
    return std::filesystem::temp_directory_path() / "nxd_mcap_writer_helpers_test.dat";
}

WriterImpl open_writer(const std::filesystem::path& path) {
    WriterImpl impl{};
    impl.file.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    return impl;
}

ReaderImpl open_reader(const std::filesystem::path& path) {
    ReaderImpl impl{};
    impl.file.open(path, std::ios::in | std::ios::binary);
    return impl;
}

int test_write_data_end() {
    const auto path = make_temp_path();
    {
        auto writer = open_writer(path);
        if (!writer.write_data_end()) return 1;
    }
    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;
    std::uint32_t crc = 1;
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size) || !reader.read_u32(crc)) return 2;
    if (opcode != kOpDataEnd || body_size != sizeof(std::uint32_t) || crc != 0) return 3;
    std::filesystem::remove(path);
    return 0;
}

int test_write_attachment_index_group() {
    const auto path = make_temp_path();
    std::uint64_t group_start = 0;
    std::uint64_t group_length = 0;
    {
        auto writer = open_writer(path);
        AttachmentEntry entry{};
        entry.name = "attachment-name";
        entry.media_type = "text/plain";
        entry.log_time = 10;
        entry.create_time = 20;
        entry.data_size = 5;
        entry.file_offset = 100;
        entry.length = 50;
        writer.attachments.push_back(entry);
        if (!writer.write_attachment_index_group(group_start, group_length)) return 1;
    }
    if (group_start != 0) return 2;

    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 3;
    if (opcode != kOpAttachmentIndex) return 4;

    const std::uint64_t expected_body_size =
        sizeof(std::uint64_t) * 5 +
        sizeof(std::uint32_t) * 2 +
        std::string("attachment-name").size() +
        std::string("text/plain").size();
    if (body_size != expected_body_size) return 5;

    std::uint64_t file_offset = 0;
    std::uint64_t length = 0;
    std::uint64_t log_time = 0;
    std::uint64_t create_time = 0;
    std::uint64_t data_size = 0;
    std::string name;
    std::string media_type;
    if (!reader.read_u64(file_offset) || !reader.read_u64(length) || !reader.read_u64(log_time) ||
        !reader.read_u64(create_time) || !reader.read_u64(data_size) || !reader.read_string(name) ||
        !reader.read_string(media_type)) {
        return 6;
    }
    if (file_offset != 100 || length != 50 || log_time != 10 || create_time != 20 || data_size != 5 ||
        name != "attachment-name" || media_type != "text/plain") {
        return 7;
    }
    if (group_length != expected_body_size + kRecordHeaderSize) return 8;
    std::filesystem::remove(path);
    return 0;
}

int test_write_attachment_index_group_empty() {
    const auto path = make_temp_path();
    std::uint64_t group_start = 0;
    std::uint64_t group_length = 0;
    {
        auto writer = open_writer(path);
        if (!writer.write_attachment_index_group(group_start, group_length)) return 1;
    }
    if (group_start != 0 || group_length != 0) return 2;
    if (std::filesystem::file_size(path) != 0) return 3;
    std::filesystem::remove(path);
    return 0;
}

int test_write_statistics() {
    const auto path = make_temp_path();
    std::uint64_t group_start = 0;
    std::uint64_t group_length = 0;
    {
        auto writer = open_writer(path);
        writer.message_count = 100;
        writer.message_start_time = 10;
        writer.message_end_time = 20;
        writer.attachments.push_back(AttachmentEntry{});
        if (!writer.write_statistics(group_start, group_length)) return 1;
    }
    if (group_start != 0) return 2;

    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 3;
    if (opcode != kOpStatistics) return 4;

    const std::uint64_t expected_body_size =
        sizeof(std::uint64_t) +     // message_count
        sizeof(std::uint16_t) +     // schema_count
        sizeof(std::uint32_t) * 5 + // channel, attachment, metadata, chunk, channel_counts counts
        sizeof(std::uint64_t) * 2;  // start/end times
    if (body_size != expected_body_size) return 5;

    Statistics stats{};
    std::uint32_t metadata_count = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t channel_counts = 1;
    if (!reader.read_u64(stats.message_count) || !reader.read_u16(stats.schema_count) ||
        !reader.read_u32(stats.channel_count) || !reader.read_u32(stats.attachment_count) ||
        !reader.read_u32(metadata_count) || !reader.read_u32(chunk_count) ||
        !reader.read_u64(stats.message_start_time) || !reader.read_u64(stats.message_end_time) ||
        !reader.read_u32(channel_counts)) {
        return 6;
    }
    if (stats.message_count != 100 || stats.schema_count != 0 || stats.channel_count != 0 ||
        stats.attachment_count != 1 || metadata_count != 0 || chunk_count != 0 ||
        stats.message_start_time != 10 || stats.message_end_time != 20 || channel_counts != 0) {
        return 7;
    }
    if (group_length != expected_body_size + kRecordHeaderSize) return 8;
    std::filesystem::remove(path);
    return 0;
}

int test_write_summary_offsets_with_attachment() {
    const auto path = make_temp_path();
    {
        auto writer = open_writer(path);
        writer.attachments.push_back(AttachmentEntry{});
        if (!writer.write_summary_offsets(10, 100, 20, 200)) return 1;
    }
    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;

    // AttachmentIndex summary offset
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 2;
    if (opcode != kOpSummaryOffset) return 3;
    std::uint8_t attachment_opcode = 0;
    std::uint64_t attachment_start = 0;
    std::uint64_t attachment_length = 0;
    if (!reader.read_u8(attachment_opcode) || !reader.read_u64(attachment_start) ||
        !reader.read_u64(attachment_length)) {
        return 4;
    }
    if (attachment_opcode != kOpAttachmentIndex || attachment_start != 10 || attachment_length != 100) {
        return 5;
    }

    // Statistics summary offset
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 6;
    if (opcode != kOpSummaryOffset) return 7;
    std::uint8_t stats_opcode = 0;
    std::uint64_t stats_start = 0;
    std::uint64_t stats_length = 0;
    if (!reader.read_u8(stats_opcode) || !reader.read_u64(stats_start) ||
        !reader.read_u64(stats_length)) {
        return 8;
    }
    if (stats_opcode != kOpStatistics || stats_start != 20 || stats_length != 200) return 9;

    std::filesystem::remove(path);
    return 0;
}

int test_write_summary_offsets_without_attachment() {
    const auto path = make_temp_path();
    {
        auto writer = open_writer(path);
        if (!writer.write_summary_offsets(10, 100, 20, 200)) return 1;
    }
    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 2;
    if (opcode != kOpSummaryOffset) return 3;
    std::uint8_t stats_opcode = 0;
    std::uint64_t stats_start = 0;
    std::uint64_t stats_length = 0;
    if (!reader.read_u8(stats_opcode) || !reader.read_u64(stats_start) ||
        !reader.read_u64(stats_length)) {
        return 4;
    }
    if (stats_opcode != kOpStatistics || stats_start != 20 || stats_length != 200) return 5;
    std::filesystem::remove(path);
    return 0;
}

int test_write_footer_and_magic() {
    const auto path = make_temp_path();
    {
        auto writer = open_writer(path);
        if (!writer.write_footer_and_magic(1234, 5678)) return 1;
    }
    auto reader = open_reader(path);
    std::uint8_t opcode = 0;
    std::uint64_t body_size = 0;
    if (!reader.read_u8(opcode) || !reader.read_u64(body_size)) return 2;
    if (opcode != kOpFooter) return 3;

    std::uint64_t summary_start = 0;
    std::uint64_t summary_offset_start = 0;
    std::uint32_t crc = 1;
    if (!reader.read_u64(summary_start) || !reader.read_u64(summary_offset_start) ||
        !reader.read_u32(crc)) {
        return 4;
    }
    if (summary_start != 1234 || summary_offset_start != 5678 || crc != 0) return 5;

    std::array<std::uint8_t, 8> magic{};
    reader.file.read(reinterpret_cast<char*>(magic.data()), magic.size());
    if (reader.file.gcount() != static_cast<std::streamsize>(magic.size()) || magic != kMcapMagic) {
        return 6;
    }
    std::filesystem::remove(path);
    return 0;
}

int test_write_data_end_bad_stream() {
    const auto path = make_temp_path();
    auto writer = open_writer(path);
    writer.file.close();
    if (writer.write_data_end()) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_write_attachment_index_group_bad_stream() {
    const auto path = make_temp_path();
    auto writer = open_writer(path);
    writer.attachments.push_back(AttachmentEntry{});
    writer.file.close();
    std::uint64_t start = 0;
    std::uint64_t length = 0;
    if (writer.write_attachment_index_group(start, length)) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_write_statistics_bad_stream() {
    const auto path = make_temp_path();
    auto writer = open_writer(path);
    writer.file.close();
    std::uint64_t start = 0;
    std::uint64_t length = 0;
    if (writer.write_statistics(start, length)) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_write_summary_offsets_bad_stream() {
    const auto path = make_temp_path();
    auto writer = open_writer(path);
    writer.file.close();
    if (writer.write_summary_offsets(1, 2, 3, 4)) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_write_footer_and_magic_bad_stream() {
    const auto path = make_temp_path();
    auto writer = open_writer(path);
    writer.file.close();
    if (writer.write_footer_and_magic(1, 2)) return 1;
    std::filesystem::remove(path);
    return 0;
}

} // namespace

int main() {
    using TestFn = int (*)();
    const TestFn tests[] = {
        test_write_data_end,
        test_write_attachment_index_group,
        test_write_attachment_index_group_empty,
        test_write_statistics,
        test_write_summary_offsets_with_attachment,
        test_write_summary_offsets_without_attachment,
        test_write_footer_and_magic,
        test_write_data_end_bad_stream,
        test_write_attachment_index_group_bad_stream,
        test_write_statistics_bad_stream,
        test_write_summary_offsets_bad_stream,
        test_write_footer_and_magic_bad_stream,
    };
    for (std::size_t i = 0; i < std::size(tests); ++i) {
        if (const int rc = tests[i]()) {
            return static_cast<int>(100 + i * 10 + rc);
        }
    }
    return 0;
}
