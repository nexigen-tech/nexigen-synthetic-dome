/* In-process smoke test for the iceoryx2-backed transport.
 *
 * Creates a publisher and subscriber for the same source name inside a single
 * process, publishes one sample, and verifies the subscriber receives it.
 */

#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unistd.h>

int main() {
    nxd::runtime::transport::IceoryxTransport transport;

    std::array<std::uint8_t, 32> hash{};
    hash[0] = 0xAB;
    hash[1] = 0xCD;

    // Create the subscriber first so that it is connected when the sample is
    // published.  The transport does not guarantee instantaneous connection
    // setup, so we retry the take with a short timeout.
    auto* subscriber = transport.create_subscriber("iox_smoke", sizeof(std::uint64_t), 1, hash);
    if (subscriber == nullptr) {
        std::cerr << "failed to create subscriber\n";
        return 1;
    }

    auto* publisher = transport.create_publisher("iox_smoke", sizeof(std::uint64_t), 1, hash);
    if (publisher == nullptr) {
        std::cerr << "failed to create publisher\n";
        return 2;
    }

    constexpr std::uint64_t expected = 0xDEADBEEFCAFEBABEULL;

    auto loan = publisher->loan();
    if (!loan.valid()) {
        std::cerr << "failed to acquire loan\n";
        return 3;
    }
    std::memcpy(loan.data, &expected, sizeof(expected));

    nxd::runtime::transport::SampleMetadata metadata{};
    metadata.frame_no = 42;
    publisher->publish(metadata);

    nxd::runtime::transport::SampleView view{};
    for (int attempt = 0; attempt < 1000; ++attempt) {
        view = subscriber->take();
        if (view.valid()) {
            break;
        }
        usleep(1000); // 1 ms
    }

    if (!view.valid()) {
        std::cerr << "subscriber did not receive a sample\n";
        return 4;
    }

    if (view.count != 1 || view.element_size != sizeof(std::uint64_t)) {
        std::cerr << "unexpected view size\n";
        return 5;
    }

    std::uint64_t received = 0;
    std::memcpy(&received, view.data, sizeof(received));
    if (received != expected) {
        std::cerr << "payload mismatch: expected " << expected << " got " << received << "\n";
        return 6;
    }

    if (view.metadata.frame_no != 42) {
        std::cerr << "metadata mismatch\n";
        return 7;
    }

    return 0;
}
