#include "nxd/runtime/composer/manifest.hpp"

#define JSON_NOEXCEPTION 1

#include <nlohmann/json.hpp>

namespace nxd::runtime::composer {

namespace {

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

[[nodiscard]] std::optional<ComponentRef>
component_ref_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ComponentRef ref;
    const auto name = j.find("name");
    const auto path = j.find("path");
    if (name == j.end() || !name->is_string() || path == j.end() || !path->is_string()) {
        return std::nullopt;
    }
    ref.name = name->get<std::string>();
    ref.path = path->get<std::string>();
    return ref;
}

[[nodiscard]] std::optional<ParameterAssignment>
parameter_assignment_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ParameterAssignment p;
    const auto name = j.find("name");
    const auto value = j.find("value");
    if (name == j.end() || !name->is_string() || value == j.end() ||
        !value->is_number_integer()) {
        return std::nullopt;
    }
    p.name = name->get<std::string>();
    p.value = value->get<std::int64_t>();
    return p;
}

[[nodiscard]] std::optional<ThreadRoleAssignment>
thread_role_assignment_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ThreadRoleAssignment t;
    const auto role = j.find("role");
    const auto thread = j.find("thread");
    if (role == j.end() || !role->is_string() || thread == j.end() || !thread->is_string()) {
        return std::nullopt;
    }
    t.role = role->get<std::string>();
    t.thread = thread->get<std::string>();
    return t;
}

[[nodiscard]] std::optional<Instance> instance_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    Instance inst;
    const auto id = j.find("id");
    const auto component_name = j.find("component_name");
    const auto partition_id = j.find("partition_id");
    if (id == j.end() || !id->is_string() || component_name == j.end() ||
        !component_name->is_string() || partition_id == j.end() ||
        !partition_id->is_number_unsigned()) {
        return std::nullopt;
    }
    inst.id = id->get<std::string>();
    inst.component_name = component_name->get<std::string>();
    inst.partition_id = partition_id->get<std::uint32_t>();

    const auto parameters = j.find("parameters");
    if (parameters != j.end()) {
        if (!parameters->is_array()) return std::nullopt;
        if (!parse_array(*parameters, inst.parameters, parameter_assignment_from_json)) {
            return std::nullopt;
        }
    }

    const auto thread_roles = j.find("thread_roles");
    if (thread_roles != j.end()) {
        if (!thread_roles->is_array()) return std::nullopt;
        if (!parse_array(*thread_roles, inst.thread_roles, thread_role_assignment_from_json)) {
            return std::nullopt;
        }
    }

    return inst;
}

[[nodiscard]] std::optional<ManifestPartition>
partition_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ManifestPartition p;
    const auto id = j.find("id");
    const auto name = j.find("name");
    if (id == j.end() || !id->is_number_unsigned() || name == j.end() ||
        !name->is_string()) {
        return std::nullopt;
    }
    p.id = id->get<std::uint32_t>();
    p.name = name->get<std::string>();
    return p;
}

[[nodiscard]] std::optional<ManifestWire> wire_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ManifestWire w;
    const auto src_inst = j.find("source_instance");
    const auto src_port = j.find("source_port");
    const auto tgt_inst = j.find("target_instance");
    const auto tgt_port = j.find("target_port");
    const auto boundary_id = j.find("let_boundary_id");
    if (src_inst == j.end() || !src_inst->is_string() || src_port == j.end() ||
        !src_port->is_string() || tgt_inst == j.end() || !tgt_inst->is_string() ||
        tgt_port == j.end() || !tgt_port->is_string() || boundary_id == j.end()) {
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

[[nodiscard]] std::optional<ManifestLetBoundary>
let_boundary_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ManifestLetBoundary b;
    if (!require_string(j, "id", b.id) ||
        !require_string(j, "source_instance", b.source_instance) ||
        !require_string(j, "source_port", b.source_port) ||
        !require_string(j, "target_instance", b.target_instance) ||
        !require_string(j, "target_port", b.target_port) ||
        !require_integer(j, "read_time_ns", b.read_time_ns) ||
        !require_integer(j, "write_deadline_ns", b.write_deadline_ns)) {
        return std::nullopt;
    }
    return b;
}

[[nodiscard]] std::optional<ManifestProcess>
process_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ManifestProcess p;
    const auto id = j.find("id");
    const auto executable = j.find("executable");
    const auto instance_ids = j.find("instance_ids");
    if (id == j.end() || !id->is_string() || executable == j.end() ||
        !executable->is_string() || instance_ids == j.end() ||
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

[[nodiscard]] std::optional<ManifestChronosConfig>
chronos_config_from_json(const nlohmann::json& j) noexcept {
    if (!j.is_object()) return std::nullopt;
    ManifestChronosConfig c;
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

} // namespace

std::optional<SystemManifest> SystemManifest::from_json(std::string_view json) {
    const nlohmann::json obj = nlohmann::json::parse(json, nullptr, false);
    if (obj.is_discarded() || !obj.is_object()) {
        return std::nullopt;
    }

    SystemManifest manifest;

    if (!parse_required_array(obj, "components", manifest.components,
                              component_ref_from_json) ||
        !parse_required_array(obj, "instances", manifest.instances, instance_from_json) ||
        !parse_required_array(obj, "partitions", manifest.partitions, partition_from_json) ||
        !parse_required_array(obj, "wires", manifest.wires, wire_from_json) ||
        !parse_required_array(obj, "let_boundaries", manifest.let_boundaries,
                              let_boundary_from_json) ||
        !parse_required_array(obj, "processes", manifest.processes, process_from_json) ||
        !parse_required_section(obj, "chronos", manifest.chronos, chronos_config_from_json)) {
        return std::nullopt;
    }

    return manifest;
}

} // namespace nxd::runtime::composer
