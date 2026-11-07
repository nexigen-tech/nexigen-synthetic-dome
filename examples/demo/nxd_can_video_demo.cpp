/* NexDome CAN + Video Demo — непрерывный режим
 *
 * Передаёт CAN и Video данные через iceoryx2 с заданным периодом.
 * Останов — Ctrl+C (SIGINT). iceoryx2 shared memory IPC.
 *
 * Сборка: cmake --build build --target nxd_demo_can_video -j$(nproc)
 * Запуск: ./build/nxd_demo_can_video
 */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/contracts/VideoFrame.hpp"
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
#include <chrono>
#include <thread>

using nxd::contracts::CanFrame;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::Codec;
using nxd::contracts::RawCanBatchClassical;
using nxd::contracts::VideoFrame;

// ── Layout hashes ──────────────────────────────────────────────────────────
static const std::array<std::uint8_t, 32> kLayoutHash{};

// ── Глобальный флаг для Ctrl+C ────────────────────────────────────────────
static volatile bool g_running = true;

extern "C" void on_sigint(int) {
    g_running = false;
}

// ── CAN расписание (повторяется каждые 5 мс) ──────────────────────────────
struct CanSched {
    std::int64_t t_ns; std::uint32_t id; Kind k; std::uint8_t dlc;
    std::array<std::uint8_t, 8> pay;
};

static const CanSched kCAN[] = {
    {0,         0x100, Kind::Data, 4, {0xDE,0xAD,0xBE,0xEF}},
    {0,         0x200, Kind::Data, 2, {0xCA,0xFE}},
    {0,         0x300, Kind::Data, 8, {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08}},
    {2'000'000, 0x100, Kind::Data, 1, {0x42}},
    {2'000'000, 0x400, Kind::Heartbeat, 0, {}},
    {4'000'000, 0x200, Kind::Error, 0, {}},
};
static constexpr std::int64_t kCANPeriod = 5'000'000; // 5 ms

// ── Параметры тайминга ────────────────────────────────────────────────────
static constexpr std::int64_t kFrameNs   = 1'000'000; // 1 ms = такт
static constexpr std::int64_t kCANIntNs  = 1'000'000; // CAN каждый такт
static constexpr std::int64_t kVideoIntNs = 20'000'000; // 20 ms = 50 FPS
static constexpr std::uint32_t kVideoW = 320, kVideoH = 240;

// ── Видео-хелперы ─────────────────────────────────────────────────────────
static std::uint32_t px(std::uint32_t x, std::uint32_t y, std::uint32_t seq) {
    return ((x ^ y) & 0xFFu) |
           (((seq * 37u + x / 8u) & 0xFFu) << 8) |
           (((y ^ (seq * 13u)) & 0xFFu) << 16) |
           (0xFFu << 24);
}

static void fill_vid(VideoFrame* f, std::uint32_t seq) {
    std::memset(f, 0, sizeof(*f));
    f->stream_id = 1; f->transport_sequence = seq; f->content_sequence = seq;
    f->width = kVideoW; f->height = kVideoH;
    f->fps_num = 50; f->fps_den = 1; f->codec = Codec::Raw;
    f->plane_strides[0] = kVideoW * 4;
    f->payload_length = kVideoW * kVideoH * 4;
    for (std::uint32_t y = 0; y < kVideoH; ++y)
        for (std::uint32_t x = 0; x < kVideoW; ++x) {
            std::uint32_t idx = (y * kVideoW + x) * 4;
            std::uint32_t c = px(x, y, seq);
            f->payload[idx+0] = c >> 0;  f->payload[idx+1] = c >> 8;
            f->payload[idx+2] = c >> 16; f->payload[idx+3] = c >> 24;
        }
}

static std::uint32_t fnv(const std::uint8_t* b, std::uint32_t l) {
    std::uint32_t h = 0x811C9DC5u;
    for (std::uint32_t i = 0; i < l; ++i) { h ^= b[i]; h *= 0x01000193u; }
    return h;
}

// ── Функция публикации CAN-пакета на заданном такте ──────────────────────
static std::size_t publish_can(
    nxd::runtime::transport::Publisher* pub, std::int64_t t, std::uint64_t fn)
{
    auto ln = pub->loan();
    if (!ln.valid()) return 0;

    auto b = reinterpret_cast<RawCanBatchClassical*>(ln.data);
    std::memset(b, 0, sizeof(*b));

    // Определяем, какой период расписания активен
    std::int64_t cycle_offset = t % kCANPeriod;
    std::size_t e = 0;
    CanFrame eb[16];

    for (const auto& s : kCAN) {
        if (s.t_ns >= cycle_offset && s.t_ns < cycle_offset + kFrameNs) {
            if (e >= 16) break;
            auto& f = eb[e++];
            std::memset(&f, 0, sizeof(f));
            f.hw_time_ns = t + static_cast<std::int64_t>(e) * 100;
            f.can_id = s.id; f.direction = Direction::SelfRx; f.kind = s.k;
            f.raw_dlc = s.dlc; f.payload_length = s.dlc; f.payload = s.pay;
        }
    }

    std::sort(eb, eb + e,
        [](const CanFrame& a, const CanFrame& b) noexcept { return a.can_id < b.can_id; });
    for (std::size_t i = 0; i < e; ++i) b->frames[i] = eb[i];
    b->count = static_cast<std::uint32_t>(e);
    b->assigned_frame = static_cast<std::uint32_t>(fn);

    nxd::runtime::transport::SampleMetadata m{};
    m.frame_no = fn; m.sim_time_ns = t;
    pub->publish(m);
    return e;
}

// ═══════════════════════════════════════════════════════════════════════════
int main() {
    signal(SIGINT, on_sigint);

    // ─── Init transport ──────────────────────────────────────────────────
    nxd::runtime::transport::IceoryxTransport tr;

    auto* can_sub = tr.create_subscriber("nxd_demo_can",
        sizeof(RawCanBatchClassical), 1, kLayoutHash);
    auto* vid_sub = tr.create_subscriber("nxd_demo_video",
        sizeof(VideoFrame), 1, kLayoutHash);
    auto* can_pub = tr.create_publisher("nxd_demo_can",
        sizeof(RawCanBatchClassical), 1, kLayoutHash);
    auto* vid_pub = tr.create_publisher("nxd_demo_video",
        sizeof(VideoFrame), 1, kLayoutHash);

    if (!can_pub || !can_sub || !vid_pub || !vid_sub) {
        std::fprintf(stderr, "FATAL: transport init failed\n"); return 1;
    }

    std::fprintf(stdout, "╔══════════════════════════════════════════════════╗\n");
    std::fprintf(stdout, "║   NexDome Demo — CAN + Video (iceoryx2)        ║\n");
    std::fprintf(stdout, "║   CAN frame:  every 1 ms (watermark if empty)  ║\n");
    std::fprintf(stdout, "║   Video frame: every 20 ms (50 FPS) 320×240    ║\n");
    std::fprintf(stdout, "║   Press Ctrl+C to stop                         ║\n");
    std::fprintf(stdout, "╚══════════════════════════════════════════════════╝\n\n");
    std::fflush(stdout);

    // ─── Состояние ───────────────────────────────────────────────────────
    std::int64_t t = 0;
    std::uint64_t fn = 0;
    std::uint64_t can_total_sent = 0;
    std::uint64_t video_total_sent = 0;
    std::uint64_t video_total_recv = 0;
    std::uint32_t video_seq = 0;
    std::uint32_t last_video_seq = 0;

    // ─── Цикл с периодическим тактом ─────────────────────────────────────
    auto next_tick = std::chrono::steady_clock::now();

    while (g_running) {
        next_tick += std::chrono::microseconds(100); // 100 µs = минимальный такт

        // ── CAN: публикуем каждый такт ───────────────────────────────────
        std::size_t can_sent = publish_can(can_pub, t, fn);
        can_total_sent += can_sent;

        // ── CAN: приём ───────────────────────────────────────────────────
        {
            auto v = can_sub->take();
            if (v.valid()) {
                auto b = reinterpret_cast<const RawCanBatchClassical*>(v.data);
                if (b->count > 0) {
                    std::fprintf(stdout, "[CAN] t=%06" PRIi64 "  %u frame(s)\n", t, b->count);
                    for (std::uint32_t i = 0; i < b->count && i < 16; ++i) {
                        auto& f = b->frames[i];
                        std::fprintf(stdout, "  ID=0x%03X dlc=%u [", f.can_id, f.raw_dlc);
                        for (std::uint8_t j = 0; j < f.payload_length && j < 8; ++j)
                            std::fprintf(stdout, "%s%02X", j ? " " : "", f.payload[j]);
                        std::fprintf(stdout, "]\n");
                    }
                }
            }
        }

        // ── Video: публикуем каждые 20 ms ────────────────────────────────
        if (t % kVideoIntNs == 0) {
            {
                auto ln = vid_pub->loan();
                if (ln.valid()) {
                    auto f = reinterpret_cast<VideoFrame*>(ln.data);
                    fill_vid(f, video_seq);
                    nxd::runtime::transport::SampleMetadata m{};
                    m.frame_no = fn; m.sim_time_ns = t;
                    vid_pub->publish(m);
                    std::fprintf(stdout, "[VID] t=%06" PRIi64 "  %ux%u seq=%u sent\n",
                                 t, kVideoW, kVideoH, video_seq);
                    video_seq++;
                    video_total_sent++;
                }
            }
            {
                auto v = vid_sub->take();
                if (v.valid()) {
                    auto f = reinterpret_cast<const VideoFrame*>(v.data);
                    if (video_total_recv > 0 && f->content_sequence != last_video_seq + 1)
                        std::fprintf(stdout, "  *** GAP: seq %u -> %u\n",
                                     last_video_seq, f->content_sequence);
                    last_video_seq = f->content_sequence;
                    std::uint32_t ck = fnv(f->payload.data(), f->payload_length);
                    std::fprintf(stdout, "[vid] t=%06" PRIi64 "  %ux%u seq=%u cksum=0x%08X\n",
                                 t, f->width, f->height, f->content_sequence, ck);
                    video_total_recv++;
                }
            }
        }

        // ── Печать статуса раз в секунду ──────────────────────────────────
        if (fn % 1000 == 0 && fn > 0) {
            std::fprintf(stdout,
                "[STAT] t=%06" PRIi64 " fn=%06" PRIu64
                " | CAN sent=%05" PRIu64 " | Video sent=%03" PRIu64 " recv=%03" PRIu64 "\n",
                t, fn, can_total_sent, video_total_sent, video_total_recv);
        }

        std::fflush(stdout);

        // ── Инкремент времени ─────────────────────────────────────────────
        t += kFrameNs;
        fn++;

        // ── Ожидание до следующего такта ─────────────────────────────────
        std::this_thread::sleep_until(next_tick);
    }

    // ─── Итог ─────────────────────────────────────────────────────────────
    std::fprintf(stdout, "\n═══════════════════════════════════════════════\n");
    std::fprintf(stdout, "  Ctrl+C received. Shutting down.\n");
    std::fprintf(stdout, "  Elapsed: %06" PRIi64 " ns (%" PRIu64 " frames)\n", t, fn);
    std::fprintf(stdout, "  CAN sent:     %" PRIu64 "\n", can_total_sent);
    std::fprintf(stdout, "  Video sent:   %" PRIu64 "\n", video_total_sent);
    std::fprintf(stdout, "  Video recv:   %" PRIu64 "\n", video_total_recv);
    std::fprintf(stdout, "═══════════════════════════════════════════════\n");
    return 0;
}