#include "mcap_reader_impl.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

namespace {

using namespace nxd::runtime::recording::mcap;

template <typename T>
void write_value(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_string(std::ofstream& out, std::string_view value) {
    write_value(out, static_cast<std::uint32_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

void write_bytes(std::ofstream& out, std::span<const std::byte> value) {
    write_value(out, static_cast<std::uint64_t>(value.size()));
    out.write(reinterpret_cast<const char*>(value.data()), static_cast<std::streamsize>(value.size()));
}

void write_bytes(std::ofstream& out, const std::vector<std::byte>& value) {
    write_bytes(out, std::span(value));
}

std::filesystem::path make_temp_path() {
    return std::filesystem::temp_directory_path() / "nxd_mcap_reader_helpers_test.dat";
}

ReaderImpl open_impl(const std::filesystem::path& path) {
    ReaderImpl impl{};
    impl.file.open(path, std::ios::in | std::ios::binary);
    return impl;
}

std::uint64_t get_file_size(const std::filesystem::path& path) {
    return static_cast<std::uint64_t>(std::filesystem::file_size(path));
}

int test_header_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_string(out, "nexigen");
        write_string(out, "nxd-recording");
    }
    auto impl = open_impl(path);
    if (!impl.read_header_body(get_file_size(path))) return 1;
    if (impl.current_offset() != get_file_size(path)) return 2;
    std::filesystem::remove(path);
    return 0;
}

int test_schema_body() {
    const auto path = make_temp_path();
    const std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(7));
        write_string(out, "schema-name");
        write_string(out, "protobuf");
        write_bytes(out, data);
    }
    auto impl = open_impl(path);
    if (!impl.read_schema_body(get_file_size(path))) return 1;
    if (impl.schemas.size() != 1) return 2;
    const auto& s = impl.schemas[0];
    if (s.id != 7 || s.name != "schema-name" || s.encoding != "protobuf" || s.data.size() != 3) {
        return 3;
    }
    if (impl.current_offset() != get_file_size(path)) return 4;
    std::filesystem::remove(path);
    return 0;
}

int test_channel_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(1));
        write_value(out, static_cast<std::uint16_t>(7));
        write_string(out, "topic.a");
        write_string(out, "nxd");
        write_value(out, static_cast<std::uint32_t>(2));
        write_string(out, "key1");
        write_string(out, "value1");
        write_string(out, "key2");
        write_string(out, "value2");
    }
    auto impl = open_impl(path);
    if (!impl.read_channel_body(get_file_size(path))) return 1;
    if (impl.channels.size() != 1) return 2;
    const auto& c = impl.channels[0];
    if (c.id != 1 || c.schema_id != 7 || c.topic != "topic.a" || c.message_encoding != "nxd") {
        return 3;
    }
    if (c.metadata.size() != 2 || c.metadata.at("key1") != "value1" || c.metadata.at("key2") != "value2") {
        return 4;
    }
    std::filesystem::remove(path);
    return 0;
}

int test_message_body() {
    const auto path = make_temp_path();
    const std::vector<std::byte> payload = {std::byte{0xAB}, std::byte{0xCD}};
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(3));
        write_value(out, static_cast<std::uint32_t>(42));
        write_value(out, static_cast<std::uint64_t>(1000));
        write_value(out, static_cast<std::uint64_t>(2000));
        write_bytes(out, payload);
    }
    auto impl = open_impl(path);
    std::uint64_t order = 5;
    if (!impl.read_message_body(get_file_size(path), order)) return 1;
    if (impl.messages.size() != 1) return 2;
    const auto& m = impl.messages[0];
    if (m.channel_id != 3 || m.sequence != 42 || m.log_time != 1000 || m.publish_time != 2000 ||
        m.data.size() != 2 || m.original_order != 5) {
        return 3;
    }
    if (order != 6) return 4;
    std::filesystem::remove(path);
    return 0;
}

int test_attachment_body() {
    const auto path = make_temp_path();
    const std::vector<std::byte> payload = {std::byte{0x12}, std::byte{0x34}};
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint64_t>(10));
        write_value(out, static_cast<std::uint64_t>(20));
        write_string(out, "attachment-name");
        write_string(out, "text/plain");
        write_bytes(out, payload);
        write_value(out, static_cast<std::uint32_t>(0xDEADBEEF));
    }
    auto impl = open_impl(path);
    if (!impl.read_attachment_body(get_file_size(path))) return 1;
    if (impl.attachments.size() != 1) return 2;
    const auto& a = impl.attachments[0];
    if (a.log_time != 10 || a.create_time != 20 || a.name != "attachment-name" ||
        a.media_type != "text/plain" || a.data.size() != 2 || a.crc != 0xDEADBEEF) {
        return 3;
    }
    std::filesystem::remove(path);
    return 0;
}

int test_statistics_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint64_t>(100)); // message_count
        write_value(out, static_cast<std::uint16_t>(2));   // schema_count
        write_value(out, static_cast<std::uint32_t>(3));   // channel_count
        write_value(out, static_cast<std::uint32_t>(4));   // attachment_count
        write_value(out, static_cast<std::uint32_t>(0));   // metadata_count
        write_value(out, static_cast<std::uint32_t>(0));   // chunk_count
        write_value(out, static_cast<std::uint64_t>(10));  // message_start_time
        write_value(out, static_cast<std::uint64_t>(20));  // message_end_time
        write_value(out, static_cast<std::uint32_t>(0));   // channel_message_counts count
    }
    auto impl = open_impl(path);
    if (!impl.read_statistics_body(get_file_size(path))) return 1;
    if (!impl.stats.has_value()) return 2;
    const auto& s = impl.stats.value();
    if (s.message_count != 100 || s.schema_count != 2 || s.channel_count != 3 ||
        s.attachment_count != 4 || s.message_start_time != 10 || s.message_end_time != 20) {
        return 3;
    }
    std::filesystem::remove(path);
    return 0;
}

int test_skip_record_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        out.write("hello world", 11);
    }
    auto impl = open_impl(path);
    if (!impl.skip_record_body(11)) return 1;
    if (impl.current_offset() != 11) return 2;
    std::filesystem::remove(path);
    return 0;
}

int test_footer_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint64_t>(1234));
        write_value(out, static_cast<std::uint64_t>(5678));
        write_value(out, static_cast<std::uint32_t>(0));
    }
    auto impl = open_impl(path);
    bool finished = false;
    if (!impl.read_footer_body(get_file_size(path), finished)) return 1;
    if (!finished) return 2;
    if (impl.current_offset() != get_file_size(path)) return 3;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_header_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_string(out, "profile");
        // missing library string
    }
    auto impl = open_impl(path);
    if (impl.read_header_body(get_file_size(path))) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_schema_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(1));
        write_string(out, "name");
        write_string(out, "enc");
        write_value(out, static_cast<std::uint64_t>(100)); // data size larger than following bytes
    }
    auto impl = open_impl(path);
    if (impl.read_schema_body(get_file_size(path))) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_channel_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(1));
        write_value(out, static_cast<std::uint16_t>(2));
        write_string(out, "topic");
        write_string(out, "enc");
        write_value(out, static_cast<std::uint32_t>(1)); // metadata_count = 1
        // missing metadata entry
    }
    auto impl = open_impl(path);
    if (impl.read_channel_body(get_file_size(path))) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_message_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint16_t>(1));
        write_value(out, static_cast<std::uint32_t>(1));
        write_value(out, static_cast<std::uint64_t>(1));
        write_value(out, static_cast<std::uint64_t>(1));
        write_value(out, static_cast<std::uint64_t>(10)); // data size larger than following bytes
    }
    auto impl = open_impl(path);
    std::uint64_t order = 0;
    if (impl.read_message_body(get_file_size(path), order)) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_attachment_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint64_t>(1));
        write_value(out, static_cast<std::uint64_t>(2));
        write_string(out, "name");
        write_string(out, "type");
        write_value(out, static_cast<std::uint64_t>(5)); // data size larger than following bytes
    }
    auto impl = open_impl(path);
    if (impl.read_attachment_body(get_file_size(path))) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_truncated_statistics_body() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        write_value(out, static_cast<std::uint64_t>(1));
        // missing rest of fields
    }
    auto impl = open_impl(path);
    if (impl.read_statistics_body(get_file_size(path))) return 1;
    std::filesystem::remove(path);
    return 0;
}

int test_skip_record_body_on_bad_stream() {
    const auto path = make_temp_path();
    {
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        out.write("hi", 2);
    }
    auto impl = open_impl(path);
    impl.file.close();
    if (impl.skip_record_body(1)) return 1;
    std::filesystem::remove(path);
    return 0;
}

} // namespace

int main() {
    using TestFn = int (*)();
    const TestFn tests[] = {
        test_header_body,
        test_schema_body,
        test_channel_body,
        test_message_body,
        test_attachment_body,
        test_statistics_body,
        test_skip_record_body,
        test_footer_body,
        test_truncated_header_body,
        test_truncated_schema_body,
        test_truncated_channel_body,
        test_truncated_message_body,
        test_truncated_attachment_body,
        test_truncated_statistics_body,
        test_skip_record_body_on_bad_stream,
    };
    for (std::size_t i = 0; i < std::size(tests); ++i) {
        if (const int rc = tests[i]()) {
            return static_cast<int>(100 + i * 10 + rc);
        }
    }
    return 0;
}
