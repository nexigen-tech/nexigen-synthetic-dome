/* Headless transport unit tests. */

#include "nxd/runtime/transport/headless_transport.hpp"

#include <array>
#include <cstdint>
#include <cstring>

namespace transport = nxd::runtime::transport;

struct Sample {
    std::uint64_t frame_no;
    std::int64_t value;
};

int main() {
    transport::HeadlessTransport t;
    std::array<std::uint8_t, 32> hash{};
    hash[0] = 0xab;

    auto* pub = t.create_publisher("CAN_A", sizeof(Sample), 1, hash);
    if (pub == nullptr) return 1;
    auto* sub_a = t.create_subscriber("CAN_A", sizeof(Sample), 1, hash);
    if (sub_a == nullptr) return 2;
    auto* sub_b = t.create_subscriber("CAN_B", sizeof(Sample), 1, hash);
    if (sub_b == nullptr) return 3;

    auto loan = pub->loan();
    if (!loan.valid()) return 4;
    auto* sample = reinterpret_cast<Sample*>(loan.data);
    sample->frame_no = 7;
    sample->value = 42;
    transport::SampleMetadata meta{};
    meta.frame_no = 7;
    meta.sim_time_ns = 1000;
    std::memcpy(meta.layout_hash.data(), hash.data(), hash.size());
    pub->publish(meta);

    auto view_a = sub_a->take();
    if (!view_a.valid()) return 10;
    auto* received = reinterpret_cast<const Sample*>(view_a.data);
    if (received->frame_no != 7 || received->value != 42) return 11;
    if (view_a.metadata.frame_no != 7) return 12;

    auto view_b = sub_b->take();
    if (view_b.valid()) return 20; // CAN_B subscriber must not see CAN_A sample.

    return 0;
}
