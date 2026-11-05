#include "nxd/runtime/composer/plan.hpp"

#define JSON_NOEXCEPTION 1

#include <nlohmann/json.hpp>

namespace nxd::runtime::composer {

namespace {

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
    const auto id = item.find("id");
    const auto src_inst = item.find("source_instance");
    const auto src_port = item.find("source_port");
    const auto tgt_inst = item.find("target_instance");
    const auto tgt_port = item.find("target_port");
    const auto read_time = item.find("read_time_ns");
    const auto write_deadline = item.find("write_deadline_ns");
    if (id == item.end() || !id->is_string() || src_inst == item.end() ||
        !src_inst->is_string() || src_port == item.end() || !src_port->is_string() ||
        tgt_inst == item.end() || !tgt_inst->is_string() ||
        tgt_port == item.end() || !tgt_port->is_string() ||
        read_time == item.end() || !read_time->is_number_integer() ||
        write_deadline == item.end() || !write_deadline->is_number_integer()) {
        return std::nullopt;
    }
    b.id = id->get<std::string>();
    b.source_instance = src_inst->get<std::string>();
    b.source_port = src_port->get<std::string>();
    b.target_instance = tgt_inst->get<std::string>();
    b.target_port = tgt_port->get<std::string>();
    b.read_time_ns = read_time->get<std::int64_t>();
    b.write_deadline_ns = write_deadline->get<std::int64_t>();
    return b;
}

[[nodiscard]] nlohmann::json partition_to_json(const Partition& p) {
    nlohmann::json j;
    j["id"] = p.id;
    j["name"] = p.name;
    j["instance_ids"] = p.instance_ids;
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

    return obj.dump(2);
}

std::optional<DomePlan> DomePlan::from_json(std::string_view json) {
    const nlohmann::json obj = nlohmann::json::parse(json, nullptr, false);
    if (obj.is_discarded() || !obj.is_object()) {
        return std::nullopt;
    }

    DomePlan plan;

    const auto version_major_it = obj.find("version_major");
    if (version_major_it == obj.end() || !version_major_it->is_number_unsigned()) {
        return std::nullopt;
    }
    plan.version_major = version_major_it->get<std::uint32_t>();

    const auto version_minor_it = obj.find("version_minor");
    if (version_minor_it == obj.end() || !version_minor_it->is_number_unsigned()) {
        return std::nullopt;
    }
    plan.version_minor = version_minor_it->get<std::uint32_t>();

    const auto chronos_it = obj.find("chronos");
    if (chronos_it == obj.end()) {
        return std::nullopt;
    }
    const auto parsed_chronos = chronos_config_from_json(*chronos_it);
    if (!parsed_chronos.has_value()) {
        return std::nullopt;
    }
    plan.chronos = *parsed_chronos;

    const auto partitions_it = obj.find("partitions");
    if (partitions_it == obj.end() || !partitions_it->is_array()) {
        return std::nullopt;
    }
    for (const auto& item : *partitions_it) {
        const auto parsed = partition_from_json(item);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        plan.partitions.push_back(*parsed);
    }

    const auto boundaries_it = obj.find("let_boundaries");
    if (boundaries_it == obj.end() || !boundaries_it->is_array()) {
        return std::nullopt;
    }
    for (const auto& item : *boundaries_it) {
        const auto parsed = let_boundary_from_json(item);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        plan.let_boundaries.push_back(*parsed);
    }

    const auto wires_it = obj.find("wires");
    if (wires_it == obj.end() || !wires_it->is_array()) {
        return std::nullopt;
    }
    for (const auto& item : *wires_it) {
        const auto parsed = wire_from_json(item);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        plan.wires.push_back(*parsed);
    }

    const auto processes_it = obj.find("processes");
    if (processes_it == obj.end() || !processes_it->is_array()) {
        return std::nullopt;
    }
    for (const auto& item : *processes_it) {
        const auto parsed = process_from_json(item);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        plan.processes.push_back(*parsed);
    }

    const auto thread_roles_it = obj.find("thread_role_assignments");
    if (thread_roles_it != obj.end()) {
        if (!thread_roles_it->is_array()) {
            return std::nullopt;
        }
        for (const auto& item : *thread_roles_it) {
            const auto parsed = thread_role_assignment_from_json(item);
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            plan.thread_role_assignments.push_back(*parsed);
        }
    }

    const auto parameters_it = obj.find("parameter_assignments");
    if (parameters_it != obj.end()) {
        if (!parameters_it->is_array()) {
            return std::nullopt;
        }
        for (const auto& item : *parameters_it) {
            const auto parsed = parameter_assignment_from_json(item);
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            plan.parameter_assignments.push_back(*parsed);
        }
    }

    return plan;
}

} // namespace nxd::runtime::composer
