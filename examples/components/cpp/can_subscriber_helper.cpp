/* CAN subscriber helper — subscribes to iceoryx2, writes RawCanBatchClassical
 * to stdout as binary blobs prefixed by a 4-byte length.
 *
 * Used by the Rust CAN_monitor which spawns this executable as a child process
 * and reads the binary stream from its stdout.
 *
 * Usage: ./nxd_can_subscriber <bus-name>
 *   bus-name: iceoryx2 service name (default: CAN_DEMO)
 *
 * Each record: 4 bytes LE uint32 = payload length, then the payload bytes.
 * Payload = RawCanBatchClassical struct (392 bytes).
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <array>
#include <cinttypes>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

using nxd::contracts::RawCanBatchClassical;
namespace tx = nxd::runtime::transport;

static volatile bool g_running = true;
extern "C" void on_sigint(int) { g_running = false; }

static const std::array<std::uint8_t, 32> kHash{};

int main(int argc, char** argv) {
    signal(SIGINT, on_sigint);
    std::string bus_name = "CAN_DEMO";
    if (argc > 1) bus_name = argv[1];

    // Write binary to stdout, text to stderr
    std::fprintf(stderr, "[subscriber] bus=%s  writing to stdout\n", bus_name.c_str());

    tx::IceoryxTransport transport;
    auto* sub = transport.create_subscriber(
        bus_name.c_str(), sizeof(RawCanBatchClassical), 1, kHash);
    if (!sub) {
        std::fprintf(stderr, "[subscriber] FATAL: cannot subscribe\n");
        return 1;
    }

    // Binary frame buffer
    std::uint8_t header[4];
    RawCanBatchClassical batch;
    std::memset(&batch, 0, sizeof(batch));
    std::uint64_t frame_count = 0;

    while (g_running) {
        auto view = sub->take();
        if (view.valid()) {
            // Copy batch from iceoryx2
            if (view.data && view.element_size == sizeof(RawCanBatchClassical) && view.count >= 1) {
                std::memcpy(&batch, view.data, sizeof(RawCanBatchClassical));

                // Write: [4-byte LE size][392-byte RawCanBatchClassical]
                std::uint32_t sz = static_cast<std::uint32_t>(sizeof(RawCanBatchClassical));
                header[0] = sz & 0xFF; header[1] = (sz >> 8) & 0xFF;
                header[2] = (sz >> 16) & 0xFF; header[3] = (sz >> 24) & 0xFF;
                std::fwrite(header, 1, 4, stdout);
                std::fwrite(&batch, 1, sizeof(batch), stdout);
                std::fflush(stdout);

                frame_count++;
                std::fprintf(stderr, "[subscriber] frame=%04" PRIu64 "  batch=%u CAN frames\n",
                            frame_count, batch.count);
            }
        } else {
            usleep(500); // 500 µs poll interval
        }
    }

    std::fprintf(stderr, "[subscriber] total frames: %" PRIu64 "\n", frame_count);
    return 0;
}