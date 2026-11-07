/* CAN_generator — standalone executable.
 *
 * Connects to an iceoryx2 CAN bus and publishes RawCanBatchClassical every 1 ms.
 * Bus name is configurable via CLI argument --can-bus.
 * Stops on SIGINT (Ctrl+C).
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>

using nxd::contracts::CanFrame;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;

static volatile bool g_running = true;
extern "C" void on_sigint(int) { g_running = false; }

static constexpr std::int64_t kFrameNs = 1'000'000;
static const std::array<std::uint8_t, 32> kHash{};

// ── CAN schedule ────────────────────────────────────────────────────────────
struct CanSched { std::int64_t t; std::uint32_t id; Kind k; std::uint8_t dlc; std::array<std::uint8_t, 8> pay; };

static const CanSched kCAN[] = {
    {0, 0x100, Kind::Data, 4, {0xDE,0xAD,0xBE,0xEF}},
    {0, 0x200, Kind::Data, 2, {0xCA,0xFE}},
    {0, 0x300, Kind::Data, 8, {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08}},
    {2'000'000, 0x100, Kind::Data, 1, {0x42}},
    {2'000'000, 0x400, Kind::Heartbeat, 0, {}},
    {4'000'000, 0x200, Kind::Error, 0, {}},
};
static constexpr std::int64_t kCANPeriod = 5'000'000;

namespace tx = nxd::runtime::transport;

// ── Publish CAN batch for current tick ──────────────────────────────────────
static std::size_t publish_can(tx::Publisher* pub, std::int64_t t, std::uint64_t fn) {
    auto ln = pub->loan();
    if (!ln.valid()) return 0;
    auto b = reinterpret_cast<RawCanBatchClassical*>(ln.data);
    std::memset(b, 0, sizeof(*b));
    std::int64_t off = t % kCANPeriod;
    std::size_t e = 0; CanFrame eb[16];
    for (const auto& s : kCAN) {
        if (s.t >= off && s.t < off + kFrameNs) {
            if (e >= 16) break;
            auto& f = eb[e++]; std::memset(&f, 0, sizeof(f));
            f.hw_time_ns = t + static_cast<std::int64_t>(e) * 100;
            f.can_id = s.id; f.direction = Direction::SelfRx; f.kind = s.k;
            f.raw_dlc = s.dlc; f.payload_length = s.dlc; f.payload = s.pay;
        }
    }
    std::sort(eb, eb + e, [](const CanFrame& a, const CanFrame& b) { return a.can_id < b.can_id; });
    for (std::size_t i = 0; i < e; ++i) b->frames[i] = eb[i];
    b->count = static_cast<std::uint32_t>(e);
    b->assigned_frame = static_cast<std::uint32_t>(fn);
    tx::SampleMetadata m{}; m.frame_no = fn; m.sim_time_ns = t;
    pub->publish(m);
    return e;
}

int main(int argc, char** argv) {
    signal(SIGINT, on_sigint);
    std::string bus_name = "CAN_DEMO";
    if (argc > 1 && std::strcmp(argv[1], "--can-bus") == 0 && argc > 2)
        bus_name = argv[2];

    std::fprintf(stdout, "[can_generator] bus=%s  publishing every 1 ms\n", bus_name.c_str());
    std::fflush(stdout);

    tx::IceoryxTransport transport;

    // Create subscriber first (iceoryx2: subscribe before publish)
    auto* subscriber = transport.create_subscriber(bus_name.c_str(), sizeof(RawCanBatchClassical), 1, kHash);
    (void)subscriber;
    auto* pub = transport.create_publisher(bus_name.c_str(), sizeof(RawCanBatchClassical), 1, kHash);
    if (!pub) { std::fprintf(stderr, "FATAL: cannot create publisher\n"); return 1; }

    std::int64_t t = 0;
    std::uint64_t fn = 0;
    std::uint64_t total = 0;
    auto next = std::chrono::steady_clock::now();

    while (g_running) {
        next += std::chrono::microseconds(100);
        total += publish_can(pub, t, fn);
        if (fn % 1000 == 0)
            fprintf(stdout, "[can_generator] t=%06" PRIi64 "  sent=%05" PRIu64 "\n", t, total);
        t += kFrameNs; fn++;
        std::this_thread::sleep_until(next);
    }
    fprintf(stdout, "\n[can_generator] total sent: %" PRIu64 "\n", total);
    return 0;
}