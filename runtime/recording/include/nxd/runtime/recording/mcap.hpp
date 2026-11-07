#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nxd::runtime::recording::mcap {

// Minimal in-process MCAP 0 writer/reader.  No chunks, no compression.  Written
// to be usable in -fno-exceptions/-fno-rtti environments.

struct Schema {
    std::uint16_t id = 0;
    std::string name;
    std::string encoding;
    std::vector<std::byte> data;
};

struct Channel {
    std::uint16_t id = 0;
    std::uint16_t schema_id = 0;
    std::string topic;
    std::string message_encoding;
    std::unordered_map<std::string, std::string> metadata;
};

struct Message {
    std::uint16_t channel_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t log_time = 0;
    std::uint64_t publish_time = 0;
    const std::byte* data = nullptr;
    std::size_t data_size = 0;
};

struct RecordedMessage {
    std::uint64_t log_time = 0;
    std::uint64_t publish_time = 0;
    std::uint16_t channel_id = 0;
    std::uint32_t sequence = 0;
    std::vector<std::byte> data;
    std::uint64_t original_order = 0;
};

struct Attachment {
    std::uint64_t log_time = 0;
    std::uint64_t create_time = 0;
    std::string name;
    std::string media_type;
    std::vector<std::byte> data;
    std::uint32_t crc = 0;
};

struct Statistics {
    std::uint64_t message_count = 0;
    std::uint16_t schema_count = 0;
    std::uint32_t channel_count = 0;
    std::uint32_t attachment_count = 0;
    std::uint64_t message_start_time = 0;
    std::uint64_t message_end_time = 0;
};

class Writer {
  public:
    Writer() = default;
    ~Writer();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    [[nodiscard]] bool open(std::string_view path) noexcept;
    [[nodiscard]] bool is_open() const noexcept;

    [[nodiscard]] bool add_schema(const Schema& schema) noexcept;
    [[nodiscard]] bool add_channel(const Channel& channel) noexcept;
    [[nodiscard]] bool write_message(const Message& message) noexcept;
    [[nodiscard]] bool add_attachment(const Attachment& attachment) noexcept;

    [[nodiscard]] bool close() noexcept;
  private:
    struct Impl;
    Impl* impl_ = nullptr;
};

class Reader {
  public:
    Reader() = default;
    ~Reader();

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    [[nodiscard]] bool open(std::string_view path) noexcept;
    [[nodiscard]] bool is_open() const noexcept;

    [[nodiscard]] const std::vector<Schema>& schemas() const noexcept;
    [[nodiscard]] const std::vector<Channel>& channels() const noexcept;
    [[nodiscard]] const std::vector<Attachment>& attachments() const noexcept;
    [[nodiscard]] const std::vector<RecordedMessage>& messages() const noexcept;
    [[nodiscard]] std::optional<Statistics> statistics() const noexcept;
  private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace nxd::runtime::recording::mcap
