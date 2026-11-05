#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace nxd::runtime::transport {

/// Metadata carried with every transported sample.
struct SampleMetadata {
    std::uint64_t frame_no = 0;
    std::int64_t sim_time_ns = 0;
    std::int64_t read_time_ns = 0;
    std::int64_t write_deadline_ns = 0;
    std::array<std::uint8_t, 32> layout_hash{};
};

/// Borrowed view of a received sample.
struct SampleView {
    const std::byte* data = nullptr;
    std::size_t element_size = 0;
    std::size_t count = 0;
    SampleMetadata metadata{};

    [[nodiscard]] bool valid() const noexcept {
        return data != nullptr;
    }
};

/// Writable loan from a publisher.
struct SampleLoan {
    std::byte* data = nullptr;
    std::size_t element_size = 0;
    std::size_t count = 0;
    SampleMetadata metadata{};

    [[nodiscard]] bool valid() const noexcept {
        return data != nullptr;
    }
};

/// Publisher endpoint for one logical source name.
class Publisher {
  public:
    virtual ~Publisher() = default;

    /// Acquire a writable loan for the next sample.  Must be published or
    /// returned before the next loan is acquired.
    [[nodiscard]] virtual SampleLoan loan() noexcept = 0;

    /// Publish the most recently acquired loan.
    virtual void publish(const SampleMetadata& metadata) noexcept = 0;
};

/// Subscriber endpoint for one logical source name.
class Subscriber {
  public:
    virtual ~Subscriber() = default;

    /// Take the latest available sample.  Returns an invalid view if no new
    /// sample has been published since the last take.
    [[nodiscard]] virtual SampleView take() noexcept = 0;
};

/// Transport backend for local multi-process or in-process sample exchange.
class Transport {
  public:
    virtual ~Transport() = default;

    /// Create or reuse a publisher for `source_name`.  The publisher is
    /// pre-allocated with room for one sample of `element_size * max_count`
    /// bytes.
    [[nodiscard]] virtual Publisher* create_publisher(std::string_view source_name,
                                                       std::size_t element_size,
                                                       std::size_t max_count,
                                                       const std::array<std::uint8_t, 32>&
                                                           layout_hash) noexcept = 0;

    /// Create or reuse a subscriber for `source_name`.  The subscriber keeps a
    /// local copy of the last sample.
    [[nodiscard]] virtual Subscriber* create_subscriber(std::string_view source_name,
                                                         std::size_t element_size,
                                                         std::size_t max_count,
                                                         const std::array<std::uint8_t, 32>&
                                                             layout_hash) noexcept = 0;
};

} // namespace nxd::runtime::transport
