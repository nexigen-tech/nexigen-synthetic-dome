/* NXD Composer — Dome Plan data model.
 *
 * The Dome Plan is the declarative artifact produced by the Composer and
 * consumed by `nexigen-run`.  It describes processes, partitions, LET
 * boundaries, wiring, thread roles, parameters, and Chronos configuration.
 */

#ifndef NXD_RUNTIME_COMPOSER_PLAN_HPP
#define NXD_RUNTIME_COMPOSER_PLAN_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nxd::runtime::composer {

/// Current Dome Plan format version.
inline constexpr std::uint32_t dome_plan_version_major = 1;
inline constexpr std::uint32_t dome_plan_version_minor = 0;

/// Logical Execution Time boundary between partitions.
///
/// A LET boundary decouples the time at which a value is produced from the
/// time at which downstream consumers read it.  The source partition must
/// write before `write_deadline_ns`; the target partition reads at
/// `read_time_ns`.
struct LetBoundary final {
    std::string id;
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    std::int64_t read_time_ns = 0;
    std::int64_t write_deadline_ns = 0;

    [[nodiscard]] bool operator==(const LetBoundary&) const noexcept = default;
};

/// Thread-role assignment resolved for a single instance.
struct PlanThreadRoleAssignment final {
    std::string instance_id;
    std::string role;
    std::string thread;

    [[nodiscard]] bool operator==(const PlanThreadRoleAssignment&) const noexcept = default;
};

/// Parameter assignment resolved for a single instance.
struct PlanParameterAssignment final {
    std::string instance_id;
    std::string name;
    std::int64_t value = 0;

    [[nodiscard]] bool operator==(const PlanParameterAssignment&) const noexcept = default;
};

/// A partition groups components that share the same frame context and may
/// communicate directly without an explicit LET boundary.
struct Partition final {
    std::uint32_t id = 0;
    std::string name;
    std::vector<std::string> instance_ids;
    std::optional<std::string> source_name;

    [[nodiscard]] bool operator==(const Partition&) const noexcept = default;
};

/// A wire connects two ports.  Wires that stay inside a partition are direct;
/// wires that cross partitions must be associated with a LET boundary.
struct Wire final {
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    std::optional<std::string> let_boundary_id;

    [[nodiscard]] bool operator==(const Wire&) const noexcept = default;
};

/// A process groups component instances that run in the same OS process.
struct Process final {
    std::string id;
    std::string executable;
    std::vector<std::string> instance_ids;

    [[nodiscard]] bool operator==(const Process&) const noexcept = default;
};

/// Chronos configuration embedded in a Dome Plan.
struct PlanChronosConfig final {
    std::uint8_t mode = 3; // Step by default, matching NxdChronosMode::Step.
    std::int64_t base_frame_interval_ns = 1'000'000;
    std::uint32_t scale_factor = 1;

    [[nodiscard]] bool operator==(const PlanChronosConfig&) const noexcept = default;
};

/// Topic subscribed by the Recorder service.
struct PlanRecordingTopic final {
    std::string source_name;
    std::size_t element_size = 0;
    std::size_t max_count = 1;
    std::array<std::uint8_t, 32> layout_hash{};
    std::string schema_name;

    [[nodiscard]] bool operator==(const PlanRecordingTopic&) const noexcept = default;
};

/// Recorder configuration embedded in a Dome Plan.
struct PlanRecordingOptions final {
    bool enabled = false;
    std::string output_path;
    std::vector<PlanRecordingTopic> topics;

    [[nodiscard]] bool operator==(const PlanRecordingOptions&) const noexcept = default;
};

/// Replay configuration embedded in a Dome Plan.
struct PlanReplayOptions final {
    bool enabled = false;
    std::string input_path;
    bool loop = false;
    double speed = 1.0;
    bool autostart = true;

    [[nodiscard]] bool operator==(const PlanReplayOptions&) const noexcept = default;
};

/// Field description used by validation rules.
struct PlanValidationFieldSpec final {
    std::string path;
    std::size_t offset = 0;
    std::size_t size = 0;
    bool is_floating = false;
    bool is_signed = false;

    [[nodiscard]] bool operator==(const PlanValidationFieldSpec&) const noexcept = default;
};

/// Validation rule declared in a Dome Plan.
struct PlanValidationRule final {
    std::string name;
    std::string topic;
    std::string blackboard_key;
    std::string op;
    PlanValidationFieldSpec field;
    PlanValidationFieldSpec reference_field;
    double constant = 0.0;
    bool has_constant = false;
    std::string constant_string;
    bool is_blackboard_rule = false;
    std::string reason_template;
    std::size_t element_size = 0;
    std::size_t max_count = 1;
    std::array<std::uint8_t, 32> layout_hash{};

    [[nodiscard]] bool operator==(const PlanValidationRule&) const noexcept = default;
};

/// Validation service configuration embedded in a Dome Plan.
struct PlanValidationOptions final {
    bool enabled = false;
    std::vector<PlanValidationRule> rules;

    [[nodiscard]] bool operator==(const PlanValidationOptions&) const noexcept = default;
};

/// Dome Plan produced by the Composer and consumed by `nexigen-run`.
struct DomePlan final {
    std::uint32_t version_major = dome_plan_version_major;
    std::uint32_t version_minor = dome_plan_version_minor;
    PlanChronosConfig chronos;
    std::vector<Partition> partitions;
    std::vector<LetBoundary> let_boundaries;
    std::vector<Wire> wires;
    std::vector<Process> processes;
    std::vector<PlanThreadRoleAssignment> thread_role_assignments;
    std::vector<PlanParameterAssignment> parameter_assignments;
    PlanRecordingOptions recording;
    PlanReplayOptions replay;
    PlanValidationOptions validation;

    /// Serializes the plan to a JSON string.
    [[nodiscard]] std::string to_json() const;

    /// Parses a Dome Plan from a JSON string.
    [[nodiscard]] static std::optional<DomePlan> from_json(std::string_view json);

    [[nodiscard]] bool operator==(const DomePlan&) const noexcept = default;
};

} // namespace nxd::runtime::composer

#endif // NXD_RUNTIME_COMPOSER_PLAN_HPP
