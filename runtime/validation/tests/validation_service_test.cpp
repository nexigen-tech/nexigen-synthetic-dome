#include "nxd/runtime/blackboard/blackboard.hpp"
#include "nxd/runtime/transport/headless_transport.hpp"
#include "nxd/runtime/transport/transport.hpp"
#include "nxd/runtime/validation/validation.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace transport = nxd::runtime::transport;
namespace validation = nxd::runtime::validation;

struct Telemetry {
    float temperature = 0.0f;
    float pressure = 0.0f;
};

struct CountSample {
    std::int32_t count = 0;
    std::uint32_t pad = 0;
};

static std::array<std::uint8_t, 32> zero_hash() noexcept {
    std::array<std::uint8_t, 32> h{};
    return h;
}

static validation::FieldSpec float_field(std::string_view path, std::size_t offset) {
    validation::FieldSpec spec;
    spec.path = path;
    spec.offset = offset;
    spec.size = sizeof(float);
    spec.is_floating = true;
    spec.is_signed = false;
    return spec;
}

static validation::FieldSpec int32_field(std::string_view path, std::size_t offset) {
    validation::FieldSpec spec;
    spec.path = path;
    spec.offset = offset;
    spec.size = sizeof(std::int32_t);
    spec.is_floating = false;
    spec.is_signed = true;
    return spec;
}

static int threshold_test() {
    const auto hash = zero_hash();
    transport::HeadlessTransport transport;
    validation::ValidationService service(transport);

    validation::Rule rule{};
    rule.name = "temperature_below_90";
    rule.topic = "telemetry";
    rule.op = validation::PredicateOp::LessThan;
    rule.field = float_field("temperature", 0);
    rule.has_constant = true;
    rule.constant = 90.0;

    if (!service.add_rule(rule, sizeof(Telemetry), 1, hash)) {
        return 1;
    }

    auto* pub = transport.create_publisher("telemetry", sizeof(Telemetry), 1, hash);
    if (pub == nullptr) {
        return 2;
    }

    const Telemetry samples[3] = {
        {80.0f, 1.0f},
        {95.0f, 1.0f},
        {85.0f, 1.0f},
    };

    for (int i = 0; i < 3; ++i) {
        auto loan = pub->loan();
        if (!loan.valid() || loan.data == nullptr) {
            return 3;
        }
        std::memcpy(loan.data, &samples[i], sizeof(Telemetry));
        transport::SampleMetadata meta{};
        meta.sim_time_ns = static_cast<std::int64_t>((i + 1) * 1000);
        meta.sequence = static_cast<std::uint32_t>(i + 1);
        pub->publish(meta);
        service.poll();
    }

    if (service.failures().size() != 1) {
        return 10;
    }
    if (service.failures()[0].offending_value != 95.0) {
        return 11;
    }
    if (service.failures()[0].sim_time_ns != 2000) {
        return 12;
    }
    if (service.pass_count() != 2) {
        return 13;
    }

    const auto result = service.finish();
    if (result.outcome != nxd::contracts::Outcome::Fail) {
        return 20;
    }
    if (result.rule_count != 1 || result.pass_count != 2 || result.fail_count != 1) {
        return 21;
    }
    if (result.failure_count != 1) {
        return 22;
    }

    return 0;
}

static int presence_test() {
    const auto hash = zero_hash();
    transport::HeadlessTransport transport;
    validation::ValidationService service(transport);

    validation::Rule rule{};
    rule.name = "temperature_present";
    rule.topic = "telemetry";
    rule.op = validation::PredicateOp::Present;
    rule.field = float_field("temperature", 0);

    if (!service.add_rule(rule, sizeof(Telemetry), 1, hash)) {
        return 30;
    }

    auto* pub = transport.create_publisher("telemetry", sizeof(Telemetry), 1, hash);
    if (pub == nullptr) {
        return 31;
    }

    Telemetry sample{55.0f, 1.0f};
    auto loan = pub->loan();
    std::memcpy(loan.data, &sample, sizeof(Telemetry));
    transport::SampleMetadata meta{};
    meta.sim_time_ns = 1000;
    meta.sequence = 1;
    pub->publish(meta);
    service.poll();

    if (!service.failures().empty()) {
        return 32;
    }
    if (service.pass_count() != 1) {
        return 33;
    }
    if (service.finish().outcome != nxd::contracts::Outcome::Pass) {
        return 34;
    }

    return 0;
}

static int field_to_constant_test() {
    const auto hash = zero_hash();
    transport::HeadlessTransport transport;
    validation::ValidationService service(transport);

    validation::Rule rule{};
    rule.name = "count_equals_42";
    rule.topic = "counts";
    rule.op = validation::PredicateOp::Equal;
    rule.field = int32_field("count", 0);
    rule.has_constant = true;
    rule.constant = 42.0;

    if (!service.add_rule(rule, sizeof(CountSample), 1, hash)) {
        return 40;
    }

    auto* pub = transport.create_publisher("counts", sizeof(CountSample), 1, hash);
    if (pub == nullptr) {
        return 41;
    }

    const CountSample samples[2] = {{42, 0}, {43, 0}};
    for (int i = 0; i < 2; ++i) {
        auto loan = pub->loan();
        std::memcpy(loan.data, &samples[i], sizeof(CountSample));
        transport::SampleMetadata meta{};
        meta.sim_time_ns = static_cast<std::int64_t>((i + 1) * 1000);
        meta.sequence = static_cast<std::uint32_t>(i + 1);
        pub->publish(meta);
        service.poll();
    }

    if (service.failures().size() != 1) {
        return 42;
    }
    if (service.failures()[0].offending_value != 43.0) {
        return 43;
    }
    if (service.pass_count() != 1) {
        return 44;
    }

    return 0;
}

static int field_to_field_test() {
    const auto hash = zero_hash();
    transport::HeadlessTransport transport;
    validation::ValidationService service(transport);

    validation::Rule rule{};
    rule.name = "temp_above_pressure";
    rule.topic = "telemetry";
    rule.op = validation::PredicateOp::GreaterThan;
    rule.field = float_field("temperature", 0);
    rule.reference_field = float_field("pressure", sizeof(float));

    if (!service.add_rule(rule, sizeof(Telemetry), 1, hash)) {
        return 50;
    }

    auto* pub = transport.create_publisher("telemetry", sizeof(Telemetry), 1, hash);
    if (pub == nullptr) {
        return 51;
    }

    const Telemetry samples[2] = {{30.0f, 20.0f}, {10.0f, 20.0f}};
    for (int i = 0; i < 2; ++i) {
        auto loan = pub->loan();
        std::memcpy(loan.data, &samples[i], sizeof(Telemetry));
        transport::SampleMetadata meta{};
        meta.sim_time_ns = static_cast<std::int64_t>((i + 1) * 1000);
        meta.sequence = static_cast<std::uint32_t>(i + 1);
        pub->publish(meta);
        service.poll();
    }

    if (service.failures().size() != 1) {
        return 52;
    }
    if (service.failures()[0].offending_value != 10.0) {
        return 53;
    }

    return 0;
}

static int blackboard_rule_test() {
    transport::HeadlessTransport transport;
    validation::ValidationService service(transport);

    validation::Rule rule{};
    rule.name = "offset_within_limit";
    rule.blackboard_key = "sensor.offset";
    rule.op = validation::PredicateOp::LessOrEqual;
    rule.field = float_field("offset", 0);
    rule.has_constant = true;
    rule.constant = 5.0;

    if (!service.add_blackboard_rule(rule)) {
        return 60;
    }

    if (service.add_blackboard_rule(validation::Rule{})) {
        return 61;
    }

    float value = 3.0f;
    service.update_blackboard_value("sensor.offset",
                                     reinterpret_cast<const std::byte*>(&value),
                                     sizeof(value), 1000);
    if (!service.failures().empty()) {
        return 62;
    }

    value = 7.0f;
    service.update_blackboard_value("sensor.offset",
                                     reinterpret_cast<const std::byte*>(&value),
                                     sizeof(value), 2000);
    if (service.failures().size() != 1) {
        return 63;
    }
    if (service.failures()[0].offending_value != 7.0) {
        return 64;
    }
    if (service.pass_count() != 1) {
        return 65;
    }

    return 0;
}

int main() {
    if (int rc = threshold_test(); rc != 0) {
        return rc;
    }
    if (int rc = presence_test(); rc != 0) {
        return rc;
    }
    if (int rc = field_to_constant_test(); rc != 0) {
        return rc;
    }
    if (int rc = field_to_field_test(); rc != 0) {
        return rc;
    }
    if (int rc = blackboard_rule_test(); rc != 0) {
        return rc;
    }
    return 0;
}
