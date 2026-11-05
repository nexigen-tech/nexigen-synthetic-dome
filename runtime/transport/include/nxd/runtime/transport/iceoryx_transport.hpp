#pragma once

#include "nxd/runtime/transport/transport.hpp"

#include <memory>

namespace nxd::runtime::transport {

/// iceoryx2-backed multi-process transport.
///
/// Uses the `iceoryx2` C bindings to exchange samples between processes via
/// shared memory.  Each logical source name maps to one iceoryx2 pub/sub
/// service.  The transport owns a single node and all publishers/subscribers
/// created through it.
class IceoryxTransport final : public Transport {
  public:
    IceoryxTransport() noexcept;
    ~IceoryxTransport() override;

    IceoryxTransport(const IceoryxTransport&) = delete;
    IceoryxTransport& operator=(const IceoryxTransport&) = delete;
    IceoryxTransport(IceoryxTransport&&) = delete;
    IceoryxTransport& operator=(IceoryxTransport&&) = delete;

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
