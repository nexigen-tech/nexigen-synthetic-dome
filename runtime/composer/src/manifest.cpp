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
        for (const auto& item : *parameters) {
            const auto parsed = parameter_assignment_from_json(item);
            if (!parsed) return std::nullopt;
            inst.parameters.push_back(*parsed);
        }
    }

    const auto thread_roles = j.find("thread_roles");
    if (thread_roles != j.end()) {
        if (!thread_roles->is_array()) return std::nullopt;
        for (const auto& item : *thread_roles) {
            const auto parsed = thread_role_assignment_from_json(item);
            if (!parsed) return std::nullopt;
            inst.thread_roles.push_back(*parsed);
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
    const auto id = j.find("id");
    const auto src_inst = j.find("source_instance");
    const auto src_port = j.find("source_port");
    const auto tgt_inst = j.find("target_instance");
    const auto tgt_port = j.find("target_port");
    const auto read_time = j.find("read_time_ns");
    const auto write_deadline = j.find("write_deadline_ns");
    if (id == j.end() || !id->is_string() || src_inst == j.end() ||
        !src_inst->is_string() || src_port == j.end() || !src_port->is_string() ||
        tgt_inst == j.end() || !tgt_inst->is_string() ||
        tgt_port == j.end() || !tgt_port->is_string() ||
        read_time == j.end() || !read_time->is_number_integer() ||
        write_deadline == j.end() || !write_deadline->is_number_integer()) {
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

    const auto components = obj.find("components");
    if (components == obj.end() || !components->is_array()) return std::nullopt;
    for (const auto& item : *components) {
        const auto parsed = component_ref_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.components.push_back(*parsed);
    }

    const auto instances = obj.find("instances");
    if (instances == obj.end() || !instances->is_array()) return std::nullopt;
    for (const auto& item : *instances) {
        const auto parsed = instance_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.instances.push_back(*parsed);
    }

    const auto partitions = obj.find("partitions");
    if (partitions == obj.end() || !partitions->is_array()) return std::nullopt;
    for (const auto& item : *partitions) {
        const auto parsed = partition_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.partitions.push_back(*parsed);
    }

    const auto wires = obj.find("wires");
    if (wires == obj.end() || !wires->is_array()) return std::nullopt;
    for (const auto& item : *wires) {
        const auto parsed = wire_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.wires.push_back(*parsed);
    }

    const auto let_boundaries = obj.find("let_boundaries");
    if (let_boundaries == obj.end() || !let_boundaries->is_array()) return std::nullopt;
    for (const auto& item : *let_boundaries) {
        const auto parsed = let_boundary_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.let_boundaries.push_back(*parsed);
    }

    const auto processes = obj.find("processes");
    if (processes == obj.end() || !processes->is_array()) return std::nullopt;
    for (const auto& item : *processes) {
        const auto parsed = process_from_json(item);
        if (!parsed) return std::nullopt;
        manifest.processes.push_back(*parsed);
    }

    const auto chronos = obj.find("chronos");
    if (chronos == obj.end()) return std::nullopt;
    const auto parsed_chronos = chronos_config_from_json(*chronos);
    if (!parsed_chronos) return std::nullopt;
    manifest.chronos = *parsed_chronos;

    return manifest;
}

} // namespace nxd::runtime::composer
