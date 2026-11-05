/* IceoryxTransport stub test.
 *
 * The iceoryx2-backed transport is not yet implemented because iceoryx2 is not
 * available as a project dependency.  This test only verifies the stub compiles
 * and reports the missing backend by returning nullptr.
 */

#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <array>
#include <cstdint>

int main() {
    nxd::runtime::transport::IceoryxTransport transport;
    std::array<std::uint8_t, 32> hash{};

    auto* pub = transport.create_publisher("CAN_A", sizeof(std::uint64_t), 1, hash);
    if (pub != nullptr) {
        return 1;
    }

    auto* sub = transport.create_subscriber("CAN_A", sizeof(std::uint64_t), 1, hash);
    if (sub != nullptr) {
        return 2;
    }

    return 0;
}
