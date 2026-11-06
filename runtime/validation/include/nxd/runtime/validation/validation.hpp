#pragma once

#include "nxd/contracts/ValidationResult.hpp"
#include "nxd/runtime/transport/transport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nxd::runtime::validation {

enum class PredicateOp : std::uint8_t {
    LessThan = 0,
    LessOrEqual,
    GreaterThan,
    GreaterOrEqual,
    Equal,
    NotEqual,
    Present,
};

struct FieldSpec {
    std::string path;
    std::size_t offset = 0;
    std::size_t size = 0;
    bool is_floating = false;
    bool is_signed = false;
};

struct Rule {
    std::string name;
    std::string topic;
    std::string blackboard_key;
    PredicateOp op = PredicateOp::LessThan;
    FieldSpec field;
    FieldSpec reference_field;
    double constant = 0.0;
    bool has_constant = false;
    std::string constant_string;
    bool is_blackboard_rule = false;
    std::string reason_template;
};

struct Failure {
    std::string rule_name;
    std::int64_t sim_time_ns = 0;
    double offending_value = 0.0;
    std::string reason;
};

// Forward declaration for private helpers.
struct RuleState;

class ValidationService {
  public:
    explicit ValidationService(transport::Transport& transport) noexcept;
    ~ValidationService();

    ValidationService(const ValidationService&) = delete;
    ValidationService& operator=(const ValidationService&) = delete;

    [[nodiscard]] bool add_rule(const Rule& rule,
                                 std::size_t element_size,
                                 std::size_t max_count,
                                 const std::array<std::uint8_t, 32>& layout_hash) noexcept;

    [[nodiscard]] bool add_blackboard_rule(const Rule& rule) noexcept;

    /// Poll subscribed topics and evaluate rules.
    void poll() noexcept;

    /// Provide a blackboard value synchronously for blackboard rules.
    void update_blackboard_value(std::string_view key,
                                  const std::byte* data,
                                  std::size_t size,
                                  std::int64_t sim_time_ns) noexcept;

    /// Build the final ValidationResult report.
    [[nodiscard]] nxd::contracts::ValidationResult finish() const noexcept;

    /// Direct read-only access to failures for tests.
    [[nodiscard]] const std::vector<Failure>& failures() const noexcept;
    [[nodiscard]] std::uint64_t pass_count() const noexcept;

  private:
    struct Impl;
    Impl* impl_ = nullptr;

    void evaluate_rule(RuleState& state,
                       const std::byte* data,
                       std::size_t size,
                       std::int64_t sim_time_ns) noexcept;
};

} // namespace nxd::runtime::validation
