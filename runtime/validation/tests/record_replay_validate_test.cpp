#include "nxd/contracts/ReplayControl.hpp"
#include "nxd/contracts/SignalValueF32.hpp"
#include "nxd/contracts/ValidationResult.hpp"
#include "nxd/runtime/recording/recorder.hpp"
#include "nxd/runtime/recording/replay.hpp"
#include "nxd/runtime/transport/headless_transport.hpp"
#include "nxd/runtime/transport/transport.hpp"
#include "nxd/runtime/validation/validation.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace recording = nxd::runtime::recording;
namespace transport = nxd::runtime::transport;
namespace validation = nxd::runtime::validation;

using SignalValueF32 = nxd::contracts::SignalValueF32;

static std::array<std::uint8_t, 32> zero_hash() noexcept {
    std::array<std::uint8_t, 32> h{};
    return h;
}

static validation::Rule make_threshold_rule() {
    validation::Rule rule{};
    rule.name = "value_below_50";
    rule.topic = "signal.f32";
    rule.op = validation::PredicateOp::LessThan;
    rule.field.path = "value";
    rule.field.offset = 0;
    rule.field.size = sizeof(float);
    rule.field.is_floating = true;
    rule.field.is_signed = false;
    rule.has_constant = true;
    rule.constant = 50.0;
    return rule;
}

static void publish_signal(transport::Publisher* pub,
                           float value,
                           std::int64_t sim_time_ns,
                           std::uint32_t sequence) {
    auto loan = pub->loan();
    SignalValueF32 sample{};
    sample.value = value;
    sample.quality = nxd::contracts::Quality::Valid;
    sample.origin = nxd::contracts::Origin::Model;
    std::memcpy(loan.data, &sample, sizeof(SignalValueF32));

    transport::SampleMetadata meta{};
    meta.sim_time_ns = sim_time_ns;
    meta.sequence = sequence;
    pub->publish(meta);
}

static bool results_equal(const nxd::contracts::ValidationResult& a,
                          const nxd::contracts::ValidationResult& b) {
    if (a.outcome != b.outcome) return false;
    if (a.rule_count != b.rule_count) return false;
    if (a.pass_count != b.pass_count) return false;
    if (a.fail_count != b.fail_count) return false;
    if (a.failure_count != b.failure_count) return false;
    const std::size_t count = a.failure_count;
    for (std::size_t i = 0; i < count; ++i) {
        if (a.failures[i].sim_time_ns != b.failures[i].sim_time_ns) return false;
        if (a.failures[i].offending_value != b.failures[i].offending_value) return false;
        if (std::memcmp(a.failures[i].rule_name.data(), b.failures[i].rule_name.data(),
                        a.failures[i].rule_name.size()) != 0) {
            return false;
        }
        if (std::memcmp(a.failures[i].reason.data(), b.failures[i].reason.data(),
                        a.failures[i].reason.size()) != 0) {
            return false;
        }
    }
    return true;
}

int main() {
    const char* path = "/tmp/opencode/record_replay_validate_test.mcap";
    const auto hash = zero_hash();

    transport::HeadlessTransport live_transport;
    recording::Recorder recorder(live_transport, path);
    if (!recorder.register_topic("signal.f32", sizeof(SignalValueF32), 1, hash,
                                 "SignalValueF32@1.0.0")) {
        return 1;
    }
    if (!recorder.start()) {
        return 2;
    }

    validation::ValidationService live_validator(live_transport);
    if (!live_validator.add_rule(make_threshold_rule(), sizeof(SignalValueF32), 1, hash)) {
        return 3;
    }

    auto* pub = live_transport.create_publisher("signal.f32", sizeof(SignalValueF32), 1, hash);
    if (pub == nullptr) {
        return 4;
    }

    const float values[3] = {10.0f, 60.0f, 30.0f};
    const std::int64_t times[3] = {1000, 2000, 3000};
    for (int i = 0; i < 3; ++i) {
        publish_signal(pub, values[i], times[i], static_cast<std::uint32_t>(i + 1));
        recorder.poll();
        live_validator.poll();
    }

    if (!recorder.stop()) {
        return 5;
    }
    const auto live_result = live_validator.finish();

    transport::HeadlessTransport replay_transport;
    recording::ReplaySource replay(replay_transport, path);
    if (!replay.load()) {
        return 10;
    }

    validation::ValidationService replay_validator(replay_transport);
    if (!replay_validator.add_rule(make_threshold_rule(), sizeof(SignalValueF32), 1, hash)) {
        return 11;
    }

    nxd::contracts::ReplayControl start_cmd{};
    start_cmd.command = nxd::contracts::Command::Start;
    replay.control(start_cmd);

    for (int i = 0; i < 3; ++i) {
        if (!replay.step(times[i])) {
            return 20 + i;
        }
        replay_validator.poll();
    }
    const auto replay_result = replay_validator.finish();

    if (live_result.outcome != nxd::contracts::Outcome::Fail) {
        return 30;
    }
    if (live_result.pass_count != 2 || live_result.fail_count != 1) {
        return 31;
    }
    if (live_result.failure_count != 1) {
        return 32;
    }
    if (live_result.failures[0].offending_value != 60.0f) {
        return 33;
    }
    if (live_result.failures[0].sim_time_ns != 2000) {
        return 34;
    }

    if (!results_equal(live_result, replay_result)) {
        return 40;
    }

    return 0;
}
