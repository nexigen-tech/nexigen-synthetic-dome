#include "nxd/contracts/BlackboardUpdate.hpp"
#include "nxd/contracts/ReplayControl.hpp"
#include "nxd/runtime/blackboard/blackboard.hpp"
#include "nxd/runtime/recording/mcap.hpp"
#include "nxd/runtime/recording/recorder.hpp"
#include "nxd/runtime/recording/replay.hpp"
#include "nxd/runtime/transport/headless_transport.hpp"
#include "nxd/runtime/transport/transport.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace recording = nxd::runtime::recording;
namespace transport = nxd::runtime::transport;

struct TestSignal {
    float value = 0.0f;
    std::uint32_t seq = 0;
};

static std::array<std::uint8_t, 32> zero_hash() noexcept {
    std::array<std::uint8_t, 32> h{};
    return h;
}

static bool publish_sample(transport::Publisher* pub,
                           const TestSignal& sample,
                           std::uint32_t sequence,
                           std::int64_t sim_time_ns) {
    auto loan = pub->loan();
    if (!loan.valid() || loan.data == nullptr) {
        return false;
    }
    std::memcpy(loan.data, &sample, sizeof(TestSignal));
    transport::SampleMetadata meta{};
    meta.sim_time_ns = sim_time_ns;
    meta.sequence = sequence;
    pub->publish(meta);
    return true;
}

static bool verify_replayed_sample(transport::Subscriber* sub,
                                   const TestSignal& expected,
                                   std::uint32_t expected_sequence,
                                   std::int64_t expected_sim_time_ns) {
    auto view = sub->take();
    if (!view.valid() || view.data == nullptr) {
        return false;
    }
    if (view.metadata.sequence != expected_sequence) {
        return false;
    }
    if (view.metadata.sim_time_ns != expected_sim_time_ns) {
        return false;
    }
    TestSignal out{};
    std::memcpy(&out, view.data, sizeof(TestSignal));
    return out.value == expected.value && out.seq == expected.seq;
}

static int record_test_samples(const char* path,
                               const std::array<std::uint8_t, 32>& hash,
                               const TestSignal samples[4]) {
    transport::HeadlessTransport transport;
    recording::Recorder recorder(transport, path);
    if (!recorder.register_topic("test.signal", sizeof(TestSignal), 1, hash, "TestSignal@1.0.0")) {
        return 1;
    }
    if (!recorder.start()) {
        return 2;
    }

    auto* pub = transport.create_publisher("test.signal", sizeof(TestSignal), 1, hash);
    if (pub == nullptr) {
        return 3;
    }

    for (int i = 0; i < 4; ++i) {
        if (!publish_sample(pub, samples[i], static_cast<std::uint32_t>(i + 1),
                            static_cast<std::int64_t>(i + 1) * 100)) {
            return 4;
        }
        recorder.poll();
    }

    if (!recorder.stop()) {
        return 5;
    }

    return 0;
}

static int replay_and_verify_samples(const char* path,
                                     const std::array<std::uint8_t, 32>& hash,
                                     const TestSignal samples[4]) {
    transport::HeadlessTransport replay_transport;
    recording::ReplaySource replay(replay_transport, path);
    if (!replay.load()) {
        return 10;
    }

    auto* sub = replay_transport.create_subscriber("test.signal", sizeof(TestSignal), 1, hash);
    if (sub == nullptr) {
        return 11;
    }

    nxd::contracts::ReplayControl start_cmd{};
    start_cmd.command = nxd::contracts::Command::Start;
    replay.control(start_cmd);
    if (!replay.is_playing()) {
        return 12;
    }

    for (int i = 0; i < 4; ++i) {
        const std::int64_t t = static_cast<std::int64_t>(i + 1) * 100;
        if (!replay.step(t)) {
            return 20 + i;
        }

        if (!verify_replayed_sample(sub, samples[i], static_cast<std::uint32_t>(i), t)) {
            return 30 + i;
        }

        if (i == 1) {
            nxd::contracts::ReplayControl pause_cmd{};
            pause_cmd.command = nxd::contracts::Command::Pause;
            replay.control(pause_cmd);
            if (replay.is_playing()) {
                return 72;
            }
            nxd::contracts::ReplayControl resume_cmd{};
            resume_cmd.command = nxd::contracts::Command::Resume;
            replay.control(resume_cmd);
            if (!replay.is_playing()) {
                return 73;
            }
        }
    }

    if (!replay.finished()) {
        return 70;
    }
    if (replay.next_sim_time_ns() != -1) {
        return 71;
    }

    nxd::contracts::ReplayControl stop_cmd{};
    stop_cmd.command = nxd::contracts::Command::Stop;
    replay.control(stop_cmd);
    if (replay.is_playing()) {
        return 74;
    }

    return 0;
}

static int record_and_replay_test() {
    const char* path = "/tmp/opencode/recorder_replay_test.mcap";
    const auto hash = zero_hash();

    const TestSignal samples[4] = {
        {1.0f, 10},
        {2.0f, 20},
        {3.0f, 30},
        {4.0f, 40},
    };

    if (int rc = record_test_samples(path, hash, samples); rc != 0) {
        return rc;
    }
    if (int rc = replay_and_verify_samples(path, hash, samples); rc != 0) {
        return rc;
    }

    return 0;
}

static int blackboard_channel_test() {
    const char* path = "/tmp/opencode/recorder_blackboard_test.mcap";
    const auto hash = zero_hash();

    transport::HeadlessTransport transport;
    recording::Recorder recorder(transport, path);
    if (!recorder.start()) {
        return 100;
    }

    auto* entry = nxd::runtime::blackboard::Blackboard::instance().create_entry(
        "test.bb", sizeof(double), hash);
    if (entry == nullptr) {
        return 101;
    }

    const double value = 3.14;
    if (entry->write(&value, sizeof(value), 7) < 0) {
        return 102;
    }

    recorder.record_blackboard();
    if (!recorder.stop()) {
        return 103;
    }

    recording::mcap::Reader reader;
    if (!reader.open(path)) {
        return 110;
    }

    std::uint16_t blackboard_channel_id = 0xFFFFu;
    bool found_channel = false;
    for (const auto& channel : reader.channels()) {
        if (channel.topic == "_blackboard") {
            found_channel = true;
            blackboard_channel_id = channel.id;
            break;
        }
    }
    if (!found_channel) {
        return 111;
    }

    bool found_update = false;
    for (const auto& msg : reader.messages()) {
        if (msg.channel_id != blackboard_channel_id) {
            continue;
        }
        if (msg.data.size() != sizeof(nxd::contracts::BlackboardUpdate)) {
            return 112;
        }
        nxd::contracts::BlackboardUpdate update{};
        std::memcpy(&update, msg.data.data(), sizeof(update));
        char name[64] = {};
        std::memcpy(name, update.entry_name.data(), update.entry_name.size());
        if (std::string_view(name) != "test.bb") {
            return 113;
        }
        if (update.revision == 0) {
            return 114;
        }
        found_update = true;
        break;
    }
    if (!found_update) {
        return 115;
    }

    return 0;
}

static int seek_and_speed_test() {
    const char* path = "/tmp/opencode/recorder_replay_test.mcap";
    const auto hash = zero_hash();

    transport::HeadlessTransport transport;
    recording::ReplaySource replay(transport, path);
    if (!replay.load()) {
        return 200;
    }

    auto* sub = transport.create_subscriber("test.signal", sizeof(TestSignal), 1, hash);
    if (sub == nullptr) {
        return 201;
    }

    nxd::contracts::ReplayControl start_cmd{};
    start_cmd.command = nxd::contracts::Command::Start;
    replay.control(start_cmd);

    nxd::contracts::ReplayControl seek_cmd{};
    seek_cmd.command = nxd::contracts::Command::Seek;
    seek_cmd.target_sim_time_ns = 250;
    replay.control(seek_cmd);

    // After seeking to 250, the next sample should be at t=300.
    if (replay.next_sim_time_ns() != 300) {
        return 202;
    }

    if (!replay.step(300)) {
        return 203;
    }
    auto view = sub->take();
    if (!view.valid() || view.data == nullptr) {
        return 204;
    }
    if (view.metadata.sim_time_ns != 300) {
        return 205;
    }

    nxd::contracts::ReplayControl speed_cmd{};
    speed_cmd.command = nxd::contracts::Command::SetSpeed;
    speed_cmd.speed = 2.0;
    replay.control(speed_cmd);

    return 0;
}

int main() {
    if (int rc = record_and_replay_test(); rc != 0) {
        return rc;
    }
    if (int rc = blackboard_channel_test(); rc != 0) {
        return rc;
    }
    if (int rc = seek_and_speed_test(); rc != 0) {
        return rc;
    }
    return 0;
}
