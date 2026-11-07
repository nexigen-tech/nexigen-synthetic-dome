#include "nxd/runtime/validation/validation.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace nxd::runtime::validation {

namespace {

[[nodiscard]] double read_field(const std::byte* data, const FieldSpec& spec) noexcept {
    if (data == nullptr || spec.size == 0) return 0.0;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data) + spec.offset;
    if (spec.is_floating) {
        if (spec.size == sizeof(float)) {
            float value = 0.0f;
            std::memcpy(&value, ptr, sizeof(value));
            return static_cast<double>(value);
        }
        if (spec.size == sizeof(double)) {
            double value = 0.0;
            std::memcpy(&value, ptr, sizeof(value));
            return value;
        }
        return 0.0;
    }
    switch (spec.size) {
    case 1:
        return spec.is_signed ? static_cast<double>(*reinterpret_cast<const std::int8_t*>(ptr)) : static_cast<double>(*ptr);
    case 2:
        return spec.is_signed ? static_cast<double>(*reinterpret_cast<const std::int16_t*>(ptr)) : static_cast<double>(*reinterpret_cast<const std::uint16_t*>(ptr));
    case 4:
        return spec.is_signed ? static_cast<double>(*reinterpret_cast<const std::int32_t*>(ptr)) : static_cast<double>(*reinterpret_cast<const std::uint32_t*>(ptr));
    case 8:
        return spec.is_signed ? static_cast<double>(*reinterpret_cast<const std::int64_t*>(ptr)) : static_cast<double>(*reinterpret_cast<const std::uint64_t*>(ptr));
    default:
        break;
    }
    return 0.0;
}

[[nodiscard]] bool evaluate_predicate(double value, const Rule& rule, double reference) noexcept {
    const double lhs = value;
    const double rhs = rule.has_constant ? rule.constant : reference;
    switch (rule.op) {
    case PredicateOp::LessThan:
        return lhs < rhs;
    case PredicateOp::LessOrEqual:
        return lhs <= rhs;
    case PredicateOp::GreaterThan:
        return lhs > rhs;
    case PredicateOp::GreaterOrEqual:
        return lhs >= rhs;
    case PredicateOp::Equal: {
        if (rule.field.is_floating) {
            return std::fabs(lhs - rhs) < 1e-6;
        }
        return lhs == rhs;
    }
    case PredicateOp::NotEqual: {
        if (rule.field.is_floating) {
            return std::fabs(lhs - rhs) >= 1e-6;
        }
        return lhs != rhs;
    }
    case PredicateOp::Present:
        return true;
    }
    return false;
}

[[nodiscard]] const char* op_name(PredicateOp op) noexcept {
    switch (op) {
    case PredicateOp::LessThan:
        return "<";
    case PredicateOp::LessOrEqual:
        return "<=";
    case PredicateOp::GreaterThan:
        return ">";
    case PredicateOp::GreaterOrEqual:
        return ">=";
    case PredicateOp::Equal:
        return "==";
    case PredicateOp::NotEqual:
        return "!=";
    case PredicateOp::Present:
        return "present";
    }
    return "?";
}

void copy_failure_name(std::array<std::uint8_t, 64>& out, std::string_view name) noexcept {
    out = {};
    const std::size_t len = std::min(name.size(), out.size());
    for (std::size_t i = 0; i < len; ++i) {
        out[i] = static_cast<std::uint8_t>(name[i]);
    }
}

void copy_failure_reason(std::array<std::uint8_t, 128>& out, std::string_view reason) noexcept {
    out = {};
    const std::size_t len = std::min(reason.size(), out.size());
    for (std::size_t i = 0; i < len; ++i) {
        out[i] = static_cast<std::uint8_t>(reason[i]);
    }
}

} // namespace

struct RuleState {
    Rule rule;
    transport::Subscriber* subscriber = nullptr;
    std::size_t element_size = 0;
    std::uint32_t last_sequence = 0;
    bool has_last_sequence = false;
    bool is_blackboard_rule = false;
    double last_blackboard_value = 0.0;
    std::int64_t last_blackboard_time = 0;
    bool has_blackboard_value = false;
};

struct ValidationService::Impl {
    transport::Transport& transport;
    std::vector<RuleState> rules;
    std::vector<Failure> failure_list;
    std::uint64_t pass_count = 0;

    explicit Impl(transport::Transport& t) : transport(t) {}
};

ValidationService::ValidationService(transport::Transport& transport) noexcept
    : impl_(new Impl{transport}) {}

ValidationService::~ValidationService() {
    delete impl_;
}

bool ValidationService::add_rule(const Rule& rule,
                                 std::size_t element_size,
                                 std::size_t max_count,
                                 const std::array<std::uint8_t, 32>& layout_hash) noexcept {
    if (impl_ == nullptr) return false;
    auto* subscriber = impl_->transport.create_subscriber(rule.topic, element_size, max_count, layout_hash);
    if (subscriber == nullptr) return false;
    RuleState state{};
    state.rule = rule;
    state.subscriber = subscriber;
    state.element_size = element_size;
    state.is_blackboard_rule = false;
    impl_->rules.push_back(std::move(state));
    return true;
}

bool ValidationService::add_blackboard_rule(const Rule& rule) noexcept {
    if (impl_ == nullptr || rule.blackboard_key.empty()) return false;
    RuleState state{};
    state.rule = rule;
    state.is_blackboard_rule = true;
    impl_->rules.push_back(std::move(state));
    return true;
}

void ValidationService::update_blackboard_value(std::string_view key,
                                                const std::byte* data,
                                                std::size_t size,
                                                std::int64_t sim_time_ns) noexcept {
    if (impl_ == nullptr || data == nullptr) return;
    for (auto& state : impl_->rules) {
        if (!state.is_blackboard_rule || state.rule.blackboard_key != key) continue;
        state.last_blackboard_value = read_field(data, state.rule.field);
        state.last_blackboard_time = sim_time_ns;
        state.has_blackboard_value = true;
        evaluate_rule(state, data, size, sim_time_ns);
    }
}

void ValidationService::evaluate_rule(RuleState& state, const std::byte* data, std::size_t size, std::int64_t sim_time_ns) noexcept {
    (void)size;
    const double reference = state.rule.reference_field.size > 0
                                 ? read_field(data, state.rule.reference_field)
                                 : 0.0;
    const double value = read_field(data, state.rule.field);
    ++impl_->pass_count;
    if (!evaluate_predicate(value, state.rule, reference)) {
        --impl_->pass_count; // failure does not count as pass
        Failure failure{};
        failure.rule_name = state.rule.name;
        failure.sim_time_ns = sim_time_ns;
        failure.offending_value = value;
        char buffer[256];
        if (state.rule.reason_template.empty()) {
            if (state.rule.op == PredicateOp::Present) {
                std::snprintf(buffer, sizeof(buffer), "field %s is missing", state.rule.field.path.c_str());
            } else if (state.rule.has_constant) {
                std::snprintf(buffer, sizeof(buffer), "value %g %s %g", value, op_name(state.rule.op), state.rule.constant);
            } else {
                std::snprintf(buffer, sizeof(buffer), "field %s (%g) %s field %s (%g)",
                              state.rule.field.path.c_str(), value, op_name(state.rule.op),
                              state.rule.reference_field.path.c_str(), reference);
            }
        } else {
            std::snprintf(buffer, sizeof(buffer), "%s", state.rule.reason_template.c_str());
        }
        failure.reason = buffer;
        impl_->failure_list.push_back(std::move(failure));
    }
}

void ValidationService::poll() noexcept {
    if (impl_ == nullptr) return;
    for (auto& state : impl_->rules) {
        if (state.is_blackboard_rule) continue;
        auto view = state.subscriber->take();
        if (!view.valid() || view.data == nullptr) continue;
        const auto seq = view.metadata.sequence;
        if (state.has_last_sequence && seq == state.last_sequence) continue;
        state.has_last_sequence = true;
        state.last_sequence = seq;
        evaluate_rule(state, view.data, state.element_size, view.metadata.sim_time_ns);
    }
}

nxd::contracts::ValidationResult ValidationService::finish() const noexcept {
    nxd::contracts::ValidationResult result{};
    result.rule_count = static_cast<std::uint32_t>(impl_->rules.size());
    const auto fail_count = impl_->failure_list.size();
    result.pass_count = impl_->pass_count;
    result.fail_count = fail_count;
    if (fail_count == 0) {
        result.outcome = nxd::contracts::Outcome::Pass;
        result.failure_count = 0;
    } else {
        result.outcome = nxd::contracts::Outcome::Fail;
        result.failure_count = static_cast<std::uint32_t>(std::min(fail_count, result.failures.size()));
        for (std::size_t i = 0; i < result.failure_count; ++i) {
            copy_failure_name(result.failures[i].rule_name, impl_->failure_list[i].rule_name);
            result.failures[i].sim_time_ns = impl_->failure_list[i].sim_time_ns;
            result.failures[i].offending_value = impl_->failure_list[i].offending_value;
            copy_failure_reason(result.failures[i].reason, impl_->failure_list[i].reason);
        }
    }
    return result;
}

const std::vector<Failure>& ValidationService::failures() const noexcept {
    static const std::vector<Failure> empty;
    return impl_ != nullptr ? impl_->failure_list : empty;
}

std::uint64_t ValidationService::pass_count() const noexcept {
    return impl_ != nullptr ? impl_->pass_count : 0;
}

} // namespace nxd::runtime::validation
