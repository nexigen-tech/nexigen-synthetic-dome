/* Headless end-to-end test for the deterministic CAN monitor. */

#include "nxd/contracts/RawCanBatchClassical.hpp"
#include "nxd/host/harness.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace host = nxd::host;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::RawCanBatchClassical;

namespace {

// Local mirrors of the generated output contracts so this TU does not include
// headers that duplicate Direction/Kind/Status enum definitions.
struct CanHeartbeatLocal {
    std::int64_t hw_time_ns;
    std::uint32_t can_id;
    std::uint8_t direction;
    std::uint8_t kind;
    std::uint8_t _pad0[2];
    std::uint32_t heartbeat_counter;
    std::uint8_t status;
    std::uint8_t _pad1[3];
};
static_assert(sizeof(CanHeartbeatLocal) == 24, "CanHeartbeat size");

struct CanAggregateErrorsLocal {
    std::int64_t detection_time_ns;
    std::int64_t heartbeat_time_ns;
    std::uint32_t rx_valid;
    std::uint32_t rx_degraded;
    std::uint32_t rx_invalid;
    std::uint32_t crc_errors;
    std::uint32_t self_rx_count;
    std::uint32_t activity_filter;
    std::uint8_t status;
    std::uint8_t _pad0[7];
};
static_assert(sizeof(CanAggregateErrorsLocal) == 48, "CanAggregateErrors size");

void make_batch(RawCanBatchClassical& batch, std::uint32_t count) {
    std::memset(&batch, 0, sizeof(RawCanBatchClassical));
    batch.count = count;
}

void add_frame(RawCanBatchClassical& batch,
               std::uint32_t index,
               std::uint32_t can_id,
               Direction direction,
               Kind kind,
               std::uint8_t length,
               std::initializer_list<std::uint8_t> payload) {
    auto& f = batch.frames[index];
    f.can_id = can_id;
    f.direction = direction;
    f.kind = kind;
    f.raw_dlc = length;
    f.payload_length = length;
    std::size_t i = 0;
    for (std::uint8_t b : payload) {
        if (i < f.payload.size()) {
            f.payload[i++] = b;
        }
    }
}

struct Harness {
    std::unique_ptr<host::HeadlessHarness> harness;
    RawCanBatchClassical* in_batch = nullptr;
    CanHeartbeatLocal* heartbeat = nullptr;
    CanAggregateErrorsLocal* aggregate = nullptr;
};

[[nodiscard]] Harness make_harness(const char* descriptor_path,
                                   const char* plugin_path,
                                   const char* config) {
    Harness h;
    h.harness = std::make_unique<host::HeadlessHarness>(plugin_path, descriptor_path);
    if (const auto st = h.harness->load(); !st) {
        std::cerr << "load failed, code=" << static_cast<int>(st.error()) << "\n";
        std::exit(2);
    }
    if (const auto st = h.harness->initialize(config); !st) {
        std::cerr << "initialize failed\n";
        std::exit(3);
    }

    std::vector<host::PortBinding> bindings;
    bindings.push_back(host::PortBinding{
        .name = "in",
        .direction = host::PortDescriptor::Direction::In,
        .element_size = sizeof(RawCanBatchClassical),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    bindings.push_back(host::PortBinding{
        .name = "heartbeat",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(CanHeartbeatLocal),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    bindings.push_back(host::PortBinding{
        .name = "aggregate",
        .direction = host::PortDescriptor::Direction::Out,
        .element_size = sizeof(CanAggregateErrorsLocal),
        .count = 1,
        .layout_hash = std::nullopt,
        .buffer = {},
    });
    h.harness->configure_ports(std::move(bindings));

    if (const auto st = h.harness->arm(42); !st) {
        std::cerr << "arm failed\n";
        std::exit(4);
    }

    h.in_batch = h.harness->port_buffer<RawCanBatchClassical>("in").data();
    h.heartbeat = h.harness->port_buffer<CanHeartbeatLocal>("heartbeat").data();
    h.aggregate = h.harness->port_buffer<CanAggregateErrorsLocal>("aggregate").data();
    if (h.in_batch == nullptr || h.heartbeat == nullptr || h.aggregate == nullptr) {
        std::cerr << "buffer lookup failed\n";
        std::exit(5);
    }
    return h;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <descriptor.json> <can-monitor-plugin.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* plugin_path = argv[2];

    // One received batch produces aggregate counters and a heartbeat.
    {
        auto h = make_harness(descriptor_path, plugin_path,
                              R"({"silence_interval_ns":2000000})");
        make_batch(*h.in_batch, 2);
        add_frame(*h.in_batch, 0, 0x100, Direction::SelfRx, Kind::Data, 4, {1, 2, 3, 4});
        add_frame(*h.in_batch, 1, 0x200, Direction::SelfRx, Kind::Error, 0, {});

        if (const auto st = h.harness->run(1); !st) {
            std::cerr << "run with batch failed\n";
            return 10;
        }

        if (h.aggregate->rx_valid != 1 || h.aggregate->rx_invalid != 1 ||
            h.aggregate->self_rx_count != 2 || h.aggregate->activity_filter != 2 ||
            h.aggregate->status != 1 /* Degraded */) {
            std::cerr << "unexpected aggregate counters\n";
            return 11;
        }
        if (h.heartbeat->can_id != 0x200 || h.heartbeat->kind != 3 /* Heartbeat */ ||
            h.heartbeat->heartbeat_counter != 1 || h.heartbeat->direction != 1 /* SelfRx */) {
            std::cerr << "unexpected heartbeat\n";
            return 12;
        }
    }

    // Silence heartbeat is emitted after the configured interval with no input.
    {
        auto h = make_harness(descriptor_path, plugin_path,
                              R"({"silence_interval_ns":2000000})");
        make_batch(*h.in_batch, 0);

        if (const auto st = h.harness->run(3); !st) {
            std::cerr << "run for silence heartbeat failed\n";
            return 20;
        }

        if (h.heartbeat->heartbeat_counter != 1) {
            std::cerr << "expected one silence heartbeat, got " << h.heartbeat->heartbeat_counter
                      << "\n";
            return 21;
        }
        if (h.aggregate->rx_valid != 0 || h.aggregate->rx_invalid != 0 ||
            h.aggregate->status != 0 /* Valid */) {
            std::cerr << "unexpected aggregate for empty frame\n";
            return 22;
        }
    }

    // Deterministic replay: same empty three-frame run produces the same outputs.
    {
        auto h1 = make_harness(descriptor_path, plugin_path,
                               R"({"silence_interval_ns":2000000})");
        make_batch(*h1.in_batch, 0);
        if (const auto st = h1.harness->run(3); !st) {
            std::cerr << "replay run 1 failed\n";
            return 30;
        }

        auto h2 = make_harness(descriptor_path, plugin_path,
                               R"({"silence_interval_ns":2000000})");
        make_batch(*h2.in_batch, 0);
        if (const auto st = h2.harness->run(3); !st) {
            std::cerr << "replay run 2 failed\n";
            return 31;
        }

        if (std::memcmp(h1.heartbeat, h2.heartbeat, sizeof(CanHeartbeatLocal)) != 0 ||
            std::memcmp(h1.aggregate, h2.aggregate, sizeof(CanAggregateErrorsLocal)) != 0) {
            std::cerr << "replay mismatch\n";
            return 32;
        }
    }

    return 0;
}
