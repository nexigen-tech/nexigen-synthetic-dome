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

int main() {
    const auto manifest = composer::SystemManifest::from_json(basic_manifest());
    if (!manifest.has_value()) return 1;

    if (manifest->components.size() != 2) return 2;
    if (manifest->instances.size() != 2) return 3;
    if (manifest->partitions.size() != 2) return 4;
    if (manifest->wires.size() != 1) return 5;
    if (manifest->let_boundaries.size() != 1) return 6;
    if (manifest->processes.size() != 1) return 7;

    if (manifest->instances[0].id != "pid_1") return 10;
    if (manifest->instances[0].partition_id != 1) return 11;
    if (manifest->instances[0].parameters.size() != 1) return 12;
    if (manifest->instances[0].parameters[0].name != "kp") return 13;
    if (manifest->instances[0].parameters[0].value != 10) return 14;
    if (manifest->instances[0].thread_roles[0].role != "control") return 15;

    const auto& boundary_id = manifest->wires[0].let_boundary_id;
    if (!boundary_id.has_value() || boundary_id.value() != "control_to_plant") {
        return 21;
    }

    if (manifest->let_boundaries[0].write_deadline_ns != 200'000) return 30;

    // Invalid JSON is rejected.
    if (composer::SystemManifest::from_json("{not json").has_value()) return 40;

    // Missing required field is rejected.
    if (composer::SystemManifest::from_json("{}").has_value()) return 41;

    return 0;
}
