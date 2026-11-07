#pragma once

#include "nxd/runtime/transport/transport.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace nxd::runtime::transport {

/// In-process transport backend for headless tests.
///
/// Each source name maps to a single shared buffer.  One publisher may write
/// per source; any number of subscribers may read the latest sample.  No heap
/// allocations occur after creation.
class HeadlessTransport final : public Transport {
  public:
    HeadlessTransport() noexcept;
    ~HeadlessTransport() override;

    HeadlessTransport(const HeadlessTransport&) = delete;
    HeadlessTransport& operator=(const HeadlessTransport&) = delete;
    HeadlessTransport(HeadlessTransport&&) = delete;
    HeadlessTransport& operator=(HeadlessTransport&&) = delete;

    [[nodiscard]] Publisher* create_publisher(std::string_view source_name,
                                              std::size_t element_size,
                                              std::size_t max_count,
                                              const std::array<std::uint8_t, 32>&
                                                  layout_hash) noexcept override;
    [[nodiscard]] Subscriber* create_subscriber(std::string_view source_name,
                                                std::size_t element_size,
                                                std::size_t max_count,
                                                const std::array<std::uint8_t, 32>&
                                                    layout_hash) noexcept override;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nxd::runtime::transport
