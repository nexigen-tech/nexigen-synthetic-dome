#include "nxd/runtime/transport/iceoryx_transport.hpp"

namespace nxd::runtime::transport {

IceoryxTransport::IceoryxTransport() noexcept = default;
IceoryxTransport::~IceoryxTransport() = default;

Publisher* IceoryxTransport::create_publisher(std::string_view /*source_name*/,
                                               std::size_t /*element_size*/,
                                               std::size_t /*max_count*/,
                                               const std::array<std::uint8_t, 32>& /*layout_hash*/)
    noexcept {
    // TODO: implement with iceoryx2 once it is available as a dependency.
    return nullptr;
}

Subscriber* IceoryxTransport::create_subscriber(std::string_view /*source_name*/,
                                                 std::size_t /*element_size*/,
                                                 std::size_t /*max_count*/,
                                                 const std::array<std::uint8_t, 32>& /*layout_hash*/)
    noexcept {
    // TODO: implement with iceoryx2 once it is available as a dependency.
    return nullptr;
}

} // namespace nxd::runtime::transport
