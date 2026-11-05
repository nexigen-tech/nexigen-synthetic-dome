#pragma once

#include "nxd/runtime/transport/transport.hpp"

namespace nxd::runtime::transport {

/// Placeholder for an iceoryx2-backed multi-process transport.
///
/// This implementation is intentionally a stub: iceoryx2 is not yet a project
/// or system dependency.  Once iceoryx2 is available, this class will create
/// publishers/subscribers keyed by source name and exchange samples between
/// processes.  Until then all creation functions return nullptr so callers can
/// detect the missing backend.
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
};

} // namespace nxd::runtime::transport
