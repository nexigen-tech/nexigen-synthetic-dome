#include "nxd/runtime/composer/plan.hpp"

#define JSON_NOEXCEPTION 1

#include <nlohmann/json.hpp>

namespace nxd::runtime::composer {

namespace {

[[nodiscard]] std::string hash_to_hex(const std::array<std::uint8_t, 32>& hash) noexcept {
    std::string out;
    out.reserve(64);
    static constexpr std::array<char, 16> hex = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (const auto b : hash) {
        out.push_back(hex[(b >> 4) & 0x0F]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

[[nodiscard]] bool hex_to_hash(const std::string& hex,
                               std::array<std::uint8_t, 32>& out) noexcept {
    out = {};
    if (hex.size() != 64) {
        return false;
    }
    const auto nibble = [](char c) -> std::optional<std::uint8_t> {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
        return std::nullopt;
    };
    for (std::size_t i = 0; i < 32; ++i) {
        const auto hi = nibble(hex[i * 2]);
        const auto lo = nibble(hex[i * 2 + 1]);
        if (!hi || !lo) return false;
        out[i] = static_cast<std::uint8_t>((*hi << 4) | *lo);
    }
    return true;
}

[[nodiscard]] nlohmann::json to_json_optional_string(
    const std::optional<std::string>& value) noexcept {
    if (value.has_value()) {
        return nlohmann::json(*value);
    }
    return nlohmann::json(nullptr);
}

[[nodiscard]] bool from_json_optional_string(const nlohmann::json& j,
                                             std::optional<std::string>& out) noexcept {
    if (j.is_null()) {
        out = std::nullopt;
        return true;
    }
    if (!j.is_string()) {
        return false;
    }
    out = j.get<std::string>();
    return true;
}

[[nodiscard]] bool parse_string_array(const nlohmann::json& j,
                                      std::vector<std::string>& out) noexcept {
    if (!j.is_array()) {
        return false;
    }
    for (const auto& item : j) {
        if (!item.is_string()) {
            return false;
        }
        out.push_back(item.get<std::string>());
    }
    return true;
}

template <typename T, typename ParseFn>
[[nodiscard]] bool parse_array(const nlohmann::json& j,
                               std::vector<T>& out,
                               const ParseFn& parse) noexcept {
    if (!j.is_array()) {
        return false;
    }
    for (const auto& item : j) {
        const auto parsed = parse(item);
        if (!parsed.has_value()) {
            return false;
        }
        out.push_back(*parsed);
    }
    return true;
}

template <typename T, typename ParseFn>
[[nodiscard]] bool parse_required_array(const nlohmann::json& obj,
                                        std::string_view key,
                                        std::vector<T>& out,
                                        const ParseFn& parse) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    return parse_array(*it, out, parse);
}

template <typename T, typename ParseFn>
[[nodiscard]] bool parse_optional_array(const nlohmann::json& obj,
                                        std::string_view key,
                                        std::vector<T>& out,
                                        const ParseFn& parse) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return true;
    }
    return parse_array(*it, out, parse);
}

template <typename T, typename ParseFn>
[[nodiscard]] bool parse_optional_section(const nlohmann::json& obj,
                                          std::string_view key,
                                          T& out,
                                          const ParseFn& parse) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return true;
    }
    const auto parsed = parse(*it);
    if (!parsed.has_value()) {
        return false;
    }
    out = *parsed;
    return true;
}

[[nodiscard]] bool parse_required_section(const nlohmann::json& obj,
                                          std::string_view key,
                                          auto& out,
                                          const auto& parse) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    const auto parsed = parse(*it);
    if (!parsed.has_value()) {
        return false;
    }
    out = *parsed;
    return true;
}

[[nodiscard]] bool require_string(const nlohmann::json& item,
                                  std::string_view key,
                                  std::string& out) noexcept;

[[nodiscard]] bool require_integer(const nlohmann::json& item,
                                   std::string_view key,
                                   std::int64_t& out) noexcept;

[[nodiscard]] nlohmann::json let_boundary_to_json(const LetBoundary& b) {
    nlohmann::json j;
    j["id"] = b.id;
    j["source_instance"] = b.source_instance;
    j["source_port"] = b.source_port;
    j["target_instance"] = b.target_instance;
    j["target_port"] = b.target_port;
    j["read_time_ns"] = b.read_time_ns;
    j["write_deadline_ns"] = b.write_deadline_ns;
    return j;
}

[[nodiscard]] std::optional<LetBoundary> let_boundary_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    LetBoundary b;
    if (!require_string(item, "id", b.id) ||
        !require_string(item, "source_instance", b.source_instance) ||
        !require_string(item, "source_port", b.source_port) ||
        !require_string(item, "target_instance", b.target_instance) ||
        !require_string(item, "target_port", b.target_port) ||
        !require_integer(item, "read_time_ns", b.read_time_ns) ||
        !require_integer(item, "write_deadline_ns", b.write_deadline_ns)) {
        return std::nullopt;
    }
    return b;
}

[[nodiscard]] nlohmann::json partition_to_json(const Partition& p) {
    nlohmann::json j;
    j["id"] = p.id;
    j["name"] = p.name;
    j["instance_ids"] = p.instance_ids;
    j["source_name"] = to_json_optional_string(p.source_name);
    return j;
}

[[nodiscard]] std::optional<Partition> partition_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    Partition p;
    const auto id = item.find("id");
    const auto name = item.find("name");
    const auto instance_ids = item.find("instance_ids");
    if (id == item.end() || !id->is_number_unsigned() || name == item.end() ||
        !name->is_string() || instance_ids == item.end() ||
        !instance_ids->is_array()) {
        return std::nullopt;
    }
    p.id = id->get<std::uint32_t>();
    p.name = name->get<std::string>();
    if (!parse_string_array(*instance_ids, p.instance_ids)) {
        return std::nullopt;
    }
    const auto source_name = item.find("source_name");
    if (source_name != item.end()) {
        if (!from_json_optional_string(*source_name, p.source_name)) {
            return std::nullopt;
        }
    }
    return p;
}

[[nodiscard]] nlohmann::json wire_to_json(const Wire& w) {
    nlohmann::json j;
    j["source_instance"] = w.source_instance;
    j["source_port"] = w.source_port;
    j["target_instance"] = w.target_instance;
    j["target_port"] = w.target_port;
    j["let_boundary_id"] = to_json_optional_string(w.let_boundary_id);
    return j;
}

[[nodiscard]] std::optional<Wire> wire_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    Wire w;
    const auto src_inst = item.find("source_instance");
    const auto src_port = item.find("source_port");
    const auto tgt_inst = item.find("target_instance");
    const auto tgt_port = item.find("target_port");
    const auto boundary_id = item.find("let_boundary_id");
    if (src_inst == item.end() || !src_inst->is_string() ||
        src_port == item.end() || !src_port->is_string() ||
        tgt_inst == item.end() || !tgt_inst->is_string() ||
        tgt_port == item.end() || !tgt_port->is_string() ||
        boundary_id == item.end()) {
        return std::nullopt;
    }
    w.source_instance = src_inst->get<std::string>();
    w.source_port = src_port->get<std::string>();
    w.target_instance = tgt_inst->get<std::string>();
    w.target_port = tgt_port->get<std::string>();
    if (!from_json_optional_string(*boundary_id, w.let_boundary_id)) {
        return std::nullopt;
    }
    return w;
}

[[nodiscard]] nlohmann::json process_to_json(const Process& p) {
    nlohmann::json j;
    j["id"] = p.id;
    j["executable"] = p.executable;
    j["instance_ids"] = p.instance_ids;
    return j;
}

[[nodiscard]] std::optional<Process> process_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    Process p;
    const auto id = item.find("id");
    const auto executable = item.find("executable");
    const auto instance_ids = item.find("instance_ids");
    if (id == item.end() || !id->is_string() || executable == item.end() ||
        !executable->is_string() || instance_ids == item.end() ||
        !instance_ids->is_array()) {
        return std::nullopt;
    }
    p.id = id->get<std::string>();
    p.executable = executable->get<std::string>();
    if (!parse_string_array(*instance_ids, p.instance_ids)) {
        return std::nullopt;
    }
    return p;
}

[[nodiscard]] nlohmann::json thread_role_assignment_to_json(const PlanThreadRoleAssignment& t) {
    nlohmann::json j;
    j["instance_id"] = t.instance_id;
    j["role"] = t.role;
    j["thread"] = t.thread;
    return j;
}

[[nodiscard]] std::optional<PlanThreadRoleAssignment>
thread_role_assignment_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    PlanThreadRoleAssignment t;
    const auto instance_id = item.find("instance_id");
    const auto role = item.find("role");
    const auto thread = item.find("thread");
    if (instance_id == item.end() || !instance_id->is_string() || role == item.end() ||
        !role->is_string() || thread == item.end() || !thread->is_string()) {
        return std::nullopt;
    }
    t.instance_id = instance_id->get<std::string>();
    t.role = role->get<std::string>();
    t.thread = thread->get<std::string>();
    return t;
}

[[nodiscard]] nlohmann::json parameter_assignment_to_json(const PlanParameterAssignment& p) {
    nlohmann::json j;
    j["instance_id"] = p.instance_id;
    j["name"] = p.name;
    j["value"] = p.value;
    return j;
}

[[nodiscard]] std::optional<PlanParameterAssignment>
parameter_assignment_from_json(const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    PlanParameterAssignment p;
    const auto instance_id = item.find("instance_id");
    const auto name = item.find("name");
    const auto value = item.find("value");
    if (instance_id == item.end() || !instance_id->is_string() || name == item.end() ||
        !name->is_string() || value == item.end() || !value->is_number_integer()) {
        return std::nullopt;
    }
    p.instance_id = instance_id->get<std::string>();
    p.name = name->get<std::string>();
    p.value = value->get<std::int64_t>();
    return p;
}

[[nodiscard]] nlohmann::json chronos_config_to_json(const PlanChronosConfig& c) {
    nlohmann::json j;
    j["mode"] = c.mode;
    j["base_frame_interval_ns"] = c.base_frame_interval_ns;
    j["scale_factor"] = c.scale_factor;
    return j;
}

[[nodiscard]] std::optional<PlanChronosConfig> chronos_config_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    PlanChronosConfig c;
    const auto mode = j.find("mode");
    const auto interval = j.find("base_frame_interval_ns");
    const auto scale = j.find("scale_factor");
    if (mode == j.end() || !mode->is_number_unsigned() ||
        interval == j.end() || !interval->is_number_integer() ||
        scale == j.end() || !scale->is_number_unsigned()) {
        return std::nullopt;
    }
    c.mode = mode->get<std::uint8_t>();
    c.base_frame_interval_ns = interval->get<std::int64_t>();
    c.scale_factor = scale->get<std::uint32_t>();
    return c;
}

[[nodiscard]] nlohmann::json recording_topic_to_json(const PlanRecordingTopic& t) {
    nlohmann::json j;
    j["source_name"] = t.source_name;
    j["element_size"] = t.element_size;
    j["max_count"] = t.max_count;
    j["layout_hash"] = hash_to_hex(t.layout_hash);
    j["schema_name"] = t.schema_name;
    return j;
}

[[nodiscard]] std::optional<PlanRecordingTopic> recording_topic_from_json(
    const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    PlanRecordingTopic t;
    const auto source_name = item.find("source_name");
    const auto element_size = item.find("element_size");
    const auto max_count = item.find("max_count");
    const auto schema_name = item.find("schema_name");
    if (source_name == item.end() || !source_name->is_string() ||
        element_size == item.end() || !element_size->is_number_unsigned() ||
        max_count == item.end() || !max_count->is_number_unsigned() ||
        schema_name == item.end() || !schema_name->is_string()) {
        return std::nullopt;
    }
    t.source_name = source_name->get<std::string>();
    t.element_size = element_size->get<std::size_t>();
    t.max_count = max_count->get<std::size_t>();
    t.schema_name = schema_name->get<std::string>();
    const auto layout_hash = item.find("layout_hash");
    if (layout_hash != item.end()) {
        if (!layout_hash->is_string() ||
            !hex_to_hash(layout_hash->get<std::string>(), t.layout_hash)) {
            return std::nullopt;
        }
    }
    return t;
}

[[nodiscard]] nlohmann::json recording_options_to_json(const PlanRecordingOptions& o) {
    nlohmann::json j;
    j["enabled"] = o.enabled;
    j["output_path"] = o.output_path;
    auto topics_json = nlohmann::json::array();
    for (const auto& t : o.topics) {
        topics_json.push_back(recording_topic_to_json(t));
    }
    j["topics"] = std::move(topics_json);
    return j;
}

[[nodiscard]] std::optional<PlanRecordingOptions> recording_options_from_json(
    const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    PlanRecordingOptions o;
    const auto enabled = j.find("enabled");
    const auto output_path = j.find("output_path");
    const auto topics = j.find("topics");
    if (enabled == j.end() || !enabled->is_boolean() ||
        output_path == j.end() || !output_path->is_string() ||
        topics == j.end() || !topics->is_array()) {
        return std::nullopt;
    }
    o.enabled = enabled->get<bool>();
    o.output_path = output_path->get<std::string>();
    if (!parse_array(*topics, o.topics, recording_topic_from_json)) {
        return std::nullopt;
    }
    return o;
}

[[nodiscard]] nlohmann::json replay_options_to_json(const PlanReplayOptions& o) {
    nlohmann::json j;
    j["enabled"] = o.enabled;
    j["input_path"] = o.input_path;
    j["loop"] = o.loop;
    j["speed"] = o.speed;
    j["autostart"] = o.autostart;
    return j;
}

[[nodiscard]] std::optional<PlanReplayOptions> replay_options_from_json(
    const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    PlanReplayOptions o;
    const auto enabled = j.find("enabled");
    const auto input_path = j.find("input_path");
    if (enabled == j.end() || !enabled->is_boolean() ||
        input_path == j.end() || !input_path->is_string()) {
        return std::nullopt;
    }
    o.enabled = enabled->get<bool>();
    o.input_path = input_path->get<std::string>();
    const auto loop = j.find("loop");
    if (loop != j.end() && loop->is_boolean()) {
        o.loop = loop->get<bool>();
    }
    const auto speed = j.find("speed");
    if (speed != j.end() && speed->is_number()) {
        o.speed = speed->get<double>();
    }
    const auto autostart = j.find("autostart");
    if (autostart != j.end() && autostart->is_boolean()) {
        o.autostart = autostart->get<bool>();
    }
    return o;
}

[[nodiscard]] nlohmann::json validation_field_spec_to_json(const PlanValidationFieldSpec& f) {
    nlohmann::json j;
    j["path"] = f.path;
    j["offset"] = f.offset;
    j["size"] = f.size;
    j["is_floating"] = f.is_floating;
    j["is_signed"] = f.is_signed;
    return j;
}

[[nodiscard]] std::optional<PlanValidationFieldSpec> validation_field_spec_from_json(
    const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    PlanValidationFieldSpec f;
    const auto path = j.find("path");
    const auto offset = j.find("offset");
    const auto size = j.find("size");
    const auto is_floating = j.find("is_floating");
    const auto is_signed = j.find("is_signed");
    if (path == j.end() || !path->is_string() ||
        offset == j.end() || !offset->is_number_unsigned() ||
        size == j.end() || !size->is_number_unsigned() ||
        is_floating == j.end() || !is_floating->is_boolean() ||
        is_signed == j.end() || !is_signed->is_boolean()) {
        return std::nullopt;
    }
    f.path = path->get<std::string>();
    f.offset = offset->get<std::size_t>();
    f.size = size->get<std::size_t>();
    f.is_floating = is_floating->get<bool>();
    f.is_signed = is_signed->get<bool>();
    return f;
}

[[nodiscard]] nlohmann::json validation_rule_to_json(const PlanValidationRule& r) {
    nlohmann::json j;
    j["name"] = r.name;
    j["topic"] = r.topic;
    j["blackboard_key"] = r.blackboard_key;
    j["op"] = r.op;
    j["field"] = validation_field_spec_to_json(r.field);
    j["reference_field"] = validation_field_spec_to_json(r.reference_field);
    j["constant"] = r.constant;
    j["has_constant"] = r.has_constant;
    j["constant_string"] = r.constant_string;
    j["is_blackboard_rule"] = r.is_blackboard_rule;
    j["reason_template"] = r.reason_template;
    j["element_size"] = r.element_size;
    j["max_count"] = r.max_count;
    j["layout_hash"] = hash_to_hex(r.layout_hash);
    return j;
}

void extract_optional_string(const nlohmann::json& item,
                             std::string_view key,
                             std::string& out) noexcept {
    const auto it = item.find(key);
    if (it != item.end() && it->is_string()) {
        out = it->get<std::string>();
    }
}

[[nodiscard]] bool require_string(const nlohmann::json& item,
                                  std::string_view key,
                                  std::string& out) noexcept {
    const auto it = item.find(key);
    if (it == item.end() || !it->is_string()) {
        return false;
    }
    out = it->get<std::string>();
    return true;
}

[[nodiscard]] bool require_integer(const nlohmann::json& item,
                                   std::string_view key,
                                   std::int64_t& out) noexcept {
    const auto it = item.find(key);
    if (it == item.end() || !it->is_number_integer()) {
        return false;
    }
    out = it->get<std::int64_t>();
    return true;
}

void extract_optional_number(const nlohmann::json& item,
                             std::string_view key,
                             double& out) noexcept {
    const auto it = item.find(key);
    if (it != item.end() && it->is_number()) {
        out = it->get<double>();
    }
}

void extract_optional_unsigned(const nlohmann::json& item,
                               std::string_view key,
                               std::size_t& out) noexcept {
    const auto it = item.find(key);
    if (it != item.end() && it->is_number_unsigned()) {
        out = it->get<std::size_t>();
    }
}

void extract_optional_bool(const nlohmann::json& item,
                           std::string_view key,
                           bool& out) noexcept {
    const auto it = item.find(key);
    if (it != item.end() && it->is_boolean()) {
        out = it->get<bool>();
    }
}

[[nodiscard]] bool extract_optional_field_spec(const nlohmann::json& item,
                                               std::string_view key,
                                               PlanValidationFieldSpec& out) noexcept {
    const auto it = item.find(key);
    if (it == item.end()) {
        return true;
    }
    const auto parsed = validation_field_spec_from_json(*it);
    if (!parsed.has_value()) {
        return false;
    }
    out = *parsed;
    return true;
}

[[nodiscard]] bool extract_optional_hash(const nlohmann::json& item,
                                         std::string_view key,
                                         std::array<std::uint8_t, 32>& out) noexcept {
    const auto it = item.find(key);
    if (it == item.end()) {
        return true;
    }
    if (!it->is_string() || !hex_to_hash(it->get<std::string>(), out)) {
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<PlanValidationRule> validation_rule_from_json(
    const nlohmann::json& item) {
    if (!item.is_object()) {
        return std::nullopt;
    }
    PlanValidationRule r;
    const auto name = item.find("name");
    const auto op = item.find("op");
    const auto field = item.find("field");
    if (name == item.end() || !name->is_string() ||
        op == item.end() || !op->is_string() ||
        field == item.end()) {
        return std::nullopt;
    }
    r.name = name->get<std::string>();
    r.op = op->get<std::string>();
    const auto parsed_field = validation_field_spec_from_json(*field);
    if (!parsed_field.has_value()) {
        return std::nullopt;
    }
    r.field = *parsed_field;

    extract_optional_string(item, "topic", r.topic);
    extract_optional_string(item, "blackboard_key", r.blackboard_key);
    if (!extract_optional_field_spec(item, "reference_field", r.reference_field)) {
        return std::nullopt;
    }
    extract_optional_number(item, "constant", r.constant);
    extract_optional_bool(item, "has_constant", r.has_constant);
    extract_optional_string(item, "constant_string", r.constant_string);
    extract_optional_bool(item, "is_blackboard_rule", r.is_blackboard_rule);
    extract_optional_string(item, "reason_template", r.reason_template);
    extract_optional_unsigned(item, "element_size", r.element_size);
    extract_optional_unsigned(item, "max_count", r.max_count);
    if (!extract_optional_hash(item, "layout_hash", r.layout_hash)) {
        return std::nullopt;
    }
    return r;
}

[[nodiscard]] nlohmann::json validation_options_to_json(const PlanValidationOptions& o) {
    nlohmann::json j;
    j["enabled"] = o.enabled;
    auto rules_json = nlohmann::json::array();
    for (const auto& r : o.rules) {
        rules_json.push_back(validation_rule_to_json(r));
    }
    j["rules"] = std::move(rules_json);
    return j;
}

[[nodiscard]] std::optional<PlanValidationOptions> validation_options_from_json(
    const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    PlanValidationOptions o;
    const auto enabled = j.find("enabled");
    const auto rules = j.find("rules");
    if (enabled == j.end() || !enabled->is_boolean() ||
        rules == j.end() || !rules->is_array()) {
        return std::nullopt;
    }
    o.enabled = enabled->get<bool>();
    if (!parse_array(*rules, o.rules, validation_rule_from_json)) {
        return std::nullopt;
    }
    return o;
}

[[nodiscard]] bool parse_version(const nlohmann::json& obj,
                                 std::uint32_t& major,
                                 std::uint32_t& minor) noexcept {
    const auto major_it = obj.find("version_major");
    if (major_it == obj.end() || !major_it->is_number_unsigned()) {
        return false;
    }
    major = major_it->get<std::uint32_t>();

    const auto minor_it = obj.find("version_minor");
    if (minor_it == obj.end() || !minor_it->is_number_unsigned()) {
        return false;
    }
    minor = minor_it->get<std::uint32_t>();
    return true;
}

} // namespace

std::string DomePlan::to_json() const {
    nlohmann::json obj;
    obj["version_major"] = version_major;
    obj["version_minor"] = version_minor;
    obj["chronos"] = chronos_config_to_json(chronos);

    auto partitions_json = nlohmann::json::array();
    for (const auto& p : partitions) {
        partitions_json.push_back(partition_to_json(p));
    }
    obj["partitions"] = std::move(partitions_json);

    auto boundaries_json = nlohmann::json::array();
    for (const auto& b : let_boundaries) {
        boundaries_json.push_back(let_boundary_to_json(b));
    }
    obj["let_boundaries"] = std::move(boundaries_json);

    auto wires_json = nlohmann::json::array();
    for (const auto& w : wires) {
        wires_json.push_back(wire_to_json(w));
    }
    obj["wires"] = std::move(wires_json);

    auto processes_json = nlohmann::json::array();
    for (const auto& p : processes) {
        processes_json.push_back(process_to_json(p));
    }
    obj["processes"] = std::move(processes_json);

    auto thread_roles_json = nlohmann::json::array();
    for (const auto& t : thread_role_assignments) {
        thread_roles_json.push_back(thread_role_assignment_to_json(t));
    }
    obj["thread_role_assignments"] = std::move(thread_roles_json);

    auto parameters_json = nlohmann::json::array();
    for (const auto& p : parameter_assignments) {
        parameters_json.push_back(parameter_assignment_to_json(p));
    }
    obj["parameter_assignments"] = std::move(parameters_json);

    obj["recording"] = recording_options_to_json(recording);
    obj["replay"] = replay_options_to_json(replay);
    obj["validation"] = validation_options_to_json(validation);

    return obj.dump(2);
}

std::optional<DomePlan> DomePlan::from_json(std::string_view json) {
    const nlohmann::json obj = nlohmann::json::parse(json, nullptr, false);
    if (obj.is_discarded() || !obj.is_object()) {
        return std::nullopt;
    }

    DomePlan plan;

    if (!parse_version(obj, plan.version_major, plan.version_minor) ||
        !parse_required_section(obj, "chronos", plan.chronos, chronos_config_from_json) ||
        !parse_required_array(obj, "partitions", plan.partitions, partition_from_json) ||
        !parse_required_array(obj, "let_boundaries", plan.let_boundaries,
                              let_boundary_from_json) ||
        !parse_required_array(obj, "wires", plan.wires, wire_from_json) ||
        !parse_required_array(obj, "processes", plan.processes, process_from_json) ||
        !parse_optional_array(obj, "thread_role_assignments", plan.thread_role_assignments,
                              thread_role_assignment_from_json) ||
        !parse_optional_array(obj, "parameter_assignments", plan.parameter_assignments,
                              parameter_assignment_from_json) ||
        !parse_optional_section(obj, "recording", plan.recording, recording_options_from_json) ||
        !parse_optional_section(obj, "replay", plan.replay, replay_options_from_json) ||
        !parse_optional_section(obj, "validation", plan.validation,
                                validation_options_from_json)) {
        return std::nullopt;
    }

    return plan;
}

} // namespace nxd::runtime::composer
