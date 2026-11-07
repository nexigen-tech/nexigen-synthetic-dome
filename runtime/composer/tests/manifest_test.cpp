/* Composer system manifest schema tests.
 *
 * Verifies that the v1 manifest schema parses correctly from representative
 * fixtures and rejects malformed input.
 */

#include "nxd/runtime/composer/manifest.hpp"

#include <string>

namespace composer = nxd::runtime::composer;

[[nodiscard]] std::string basic_manifest() {
    return R"({
  "components": [
    {"name": "pid", "path": "components/pid.nxdd"},
    {"name": "plant", "path": "components/plant.nxdd"}
  ],
  "instances": [
    {
      "id": "pid_1",
      "component_name": "pid",
      "partition_id": 1,
      "parameters": [{"name": "kp", "value": 10}],
      "thread_roles": [{"role": "control", "thread": "t0"}]
    },
    {
      "id": "plant_1",
      "component_name": "plant",
      "partition_id": 2,
      "parameters": [],
      "thread_roles": []
    }
  ],
  "partitions": [
    {"id": 1, "name": "control"},
    {"id": 2, "name": "plant"}
  ],
  "wires": [
    {
      "source_instance": "pid_1",
      "source_port": "command",
      "target_instance": "plant_1",
      "target_port": "setpoint",
      "let_boundary_id": "control_to_plant"
    }
  ],
  "let_boundaries": [
    {
      "id": "control_to_plant",
      "source_instance": "pid_1",
      "source_port": "command",
      "target_instance": "plant_1",
      "target_port": "setpoint",
      "read_time_ns": 0,
      "write_deadline_ns": 200000
    }
  ],
  "processes": [
    {
      "id": "p0",
      "executable": "./nxd_process",
      "instance_ids": ["pid_1", "plant_1"]
    }
  ],
  "chronos": {
    "mode": 3,
    "base_frame_interval_ns": 1000000,
    "scale_factor": 1
  }
})";
}

static bool parsed_manifest_has_expected_structure(const composer::SystemManifest& m) {
    if (m.components.size() != 2) return false;
    if (m.instances.size() != 2) return false;
    if (m.partitions.size() != 2) return false;
    if (m.wires.size() != 1) return false;
    if (m.let_boundaries.size() != 1) return false;
    if (m.processes.size() != 1) return false;
    return true;
}

static bool parsed_manifest_has_expected_pid_instance(const composer::SystemManifest& m) {
    if (m.instances[0].id != "pid_1") return false;
    if (m.instances[0].partition_id != 1) return false;
    if (m.instances[0].parameters.size() != 1) return false;
    if (m.instances[0].parameters[0].name != "kp") return false;
    if (m.instances[0].parameters[0].value != 10) return false;
    if (m.instances[0].thread_roles[0].role != "control") return false;
    return true;
}

static bool parsed_manifest_has_expected_wire_and_boundary(const composer::SystemManifest& m) {
    const auto& boundary_id = m.wires[0].let_boundary_id;
    if (!boundary_id.has_value() || boundary_id.value() != "control_to_plant") {
        return false;
    }
    if (m.let_boundaries[0].write_deadline_ns != 200'000) return false;
    return true;
}

static bool test_basic_manifest_parses() {
    const auto manifest = composer::SystemManifest::from_json(basic_manifest());
    if (!manifest.has_value()) return false;
    if (!parsed_manifest_has_expected_structure(*manifest)) return false;
    if (!parsed_manifest_has_expected_pid_instance(*manifest)) return false;
    if (!parsed_manifest_has_expected_wire_and_boundary(*manifest)) return false;
    return true;
}

static bool test_invalid_json_rejected() {
    if (composer::SystemManifest::from_json("{not json").has_value()) return false;
    return true;
}

static bool test_missing_required_field_rejected() {
    if (composer::SystemManifest::from_json("{}").has_value()) return false;
    return true;
}

static bool test_missing_required_section_rejected() {
    if (composer::SystemManifest::from_json(R"({
      "components": [],
      "instances": [],
      "partitions": [],
      "wires": [],
      "let_boundaries": [],
      "processes": []
    })")
            .has_value()) {
        return false;
    }
    return true;
}

static bool test_wrong_type_for_required_array_rejected() {
    auto json = basic_manifest();
    json.replace(json.find("\"partitions\": ["), 16, "\"partitions\": \"not-an-array\",");
    if (composer::SystemManifest::from_json(json).has_value()) return false;
    return true;
}

static bool test_malformed_component_element_rejected() {
    if (composer::SystemManifest::from_json(R"({
      "components": [{"name": "pid"}],
      "instances": [], "partitions": [], "wires": [],
      "let_boundaries": [], "processes": [],
      "chronos": {"mode": 3, "base_frame_interval_ns": 1000000, "scale_factor": 1}
    })")
            .has_value()) {
        return false;
    }
    return true;
}

static bool test_malformed_instance_element_rejected() {
    if (composer::SystemManifest::from_json(R"({
      "components": [],
      "instances": [{"id": "pid_1"}],
      "partitions": [], "wires": [],
      "let_boundaries": [], "processes": [],
      "chronos": {"mode": 3, "base_frame_interval_ns": 1000000, "scale_factor": 1}
    })")
            .has_value()) {
        return false;
    }
    return true;
}

static bool test_malformed_wire_element_rejected() {
    if (composer::SystemManifest::from_json(R"({
      "components": [],
      "instances": [],
      "partitions": [],
      "wires": [{"source_instance": "a"}],
      "let_boundaries": [], "processes": [],
      "chronos": {"mode": 3, "base_frame_interval_ns": 1000000, "scale_factor": 1}
    })")
            .has_value()) {
        return false;
    }
    return true;
}

int main() {
    if (!test_basic_manifest_parses()) return 1;
    if (!test_invalid_json_rejected()) return 2;
    if (!test_missing_required_field_rejected()) return 3;
    if (!test_missing_required_section_rejected()) return 4;
    if (!test_wrong_type_for_required_array_rejected()) return 5;
    if (!test_malformed_component_element_rejected()) return 6;
    if (!test_malformed_instance_element_rejected()) return 7;
    if (!test_malformed_wire_element_rejected()) return 8;

    return 0;
}
