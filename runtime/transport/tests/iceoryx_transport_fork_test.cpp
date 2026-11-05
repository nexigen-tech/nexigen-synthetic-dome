/* Multi-process walking-skeleton test for the iceoryx2-backed transport.
 *
 * Forks into a publisher child and a subscriber parent, exchanges one
 * deterministic sample over a source name unique to this process, and compares
 * the received bytes.
 */

#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr std::uint64_t expected = 0x1122'3344'5566'7788ULL;

int run_publisher(const std::string& source_name) {
    nxd::runtime::transport::IceoryxTransport transport;

    std::array<std::uint8_t, 32> hash{};
    hash[0] = 0x12;
    hash[1] = 0x34;

    auto* publisher = transport.create_publisher(source_name, sizeof(std::uint64_t), 1, hash);
    if (publisher == nullptr) {
        std::cerr << "publisher: failed to create publisher\n";
        return 1;
    }

    nxd::runtime::transport::SampleMetadata metadata{};
    metadata.frame_no = 7;
    std::memcpy(metadata.layout_hash.data(), hash.data(), hash.size());

    // Publish repeatedly for a short window so that a late-connecting
    // subscriber on the other side still receives the sample.
    for (int attempt = 0; attempt < 500; ++attempt) {
        auto loan = publisher->loan();
        if (loan.valid()) {
            std::memcpy(loan.data, &expected, sizeof(expected));
            publisher->publish(metadata);
        }
        usleep(1000); // 1 ms
    }

    return 0;
}

int run_subscriber(const std::string& source_name) {
    nxd::runtime::transport::IceoryxTransport transport;

    std::array<std::uint8_t, 32> hash{};
    hash[0] = 0x12;
    hash[1] = 0x34;

    auto* subscriber = transport.create_subscriber(source_name, sizeof(std::uint64_t), 1, hash);
    if (subscriber == nullptr) {
        std::cerr << "subscriber: failed to create subscriber\n";
        return 10;
    }

    nxd::runtime::transport::SampleView view{};
    for (int attempt = 0; attempt < 2000; ++attempt) {
        view = subscriber->take();
        if (view.valid()) {
            break;
        }
        usleep(1000); // 1 ms
    }

    if (!view.valid()) {
        std::cerr << "subscriber: did not receive a sample\n";
        return 11;
    }

    if (view.count != 1 || view.element_size != sizeof(std::uint64_t)) {
        std::cerr << "subscriber: unexpected view size\n";
        return 12;
    }

    std::uint64_t received = 0;
    std::memcpy(&received, view.data, sizeof(received));
    if (received != expected) {
        std::cerr << "subscriber: payload mismatch\n";
        return 13;
    }

    if (view.metadata.frame_no != 7) {
        std::cerr << "subscriber: metadata mismatch\n";
        return 14;
    }

    return 0;
}

} // namespace

int main() {
    const std::string source_name = "iox_fork_" + std::to_string(getpid());

    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    if (child == 0) {
        const int rc = run_publisher(source_name);
        _exit(rc);
    }

    const int sub_rc = run_subscriber(source_name);

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        std::cerr << "waitpid failed\n";
        return 2;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "publisher child failed\n";
        return 3;
    }

    return sub_rc;
}
