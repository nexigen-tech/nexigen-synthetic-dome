/* Component descriptor JSON parsing tests.
 *
 * Verifies that load_component_descriptor accepts valid descriptors and rejects
 * malformed or missing fields. These cases exercise the refactored helpers in
 * host/src/descriptor.cpp.
 */

#include "nxd/host/descriptor.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path write_temp(const char* name, std::string_view content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary);
    file << content;
    return path;
}

bool valid_descriptor_loads() {
    const auto path = write_temp("nxd_valid_descriptor.json", R"({
        "name": "test.counter",
        "version": "1.2.3",
        "sdk": {"abi_major": 1, "abi_minor": 0},
        "contracts": [],
        "ports": [
            {
                "name": "count",
                "direction": "out",
                "semantics": "sample",
                "contract": "Counter",
                "contract_version": "1.0.0",
                "required": true
            }
        ],
        "parameters": [
            {"name": "limit", "type": "int64", "required": true}
        ],
        "thread_roles": [
            {"name": "worker", "required": true}
        ],
        "resources": {"max_heap_bytes": 65536, "deterministic": true},
        "supported_modes": ["default"]
    })");

    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (!descriptor) {
        std::cerr << "valid_descriptor_loads: failed\n";
        return false;
    }
    if (descriptor->name != "test.counter" || descriptor->version.major != 1 ||
        descriptor->version.minor != 2 || descriptor->version.patch != 3) {
        std::cerr << "valid_descriptor_loads: metadata mismatch\n";
        return false;
    }
    if (descriptor->sdk.abi_major != 1 || descriptor->sdk.abi_minor != 0) {
        std::cerr << "valid_descriptor_loads: sdk mismatch\n";
        return false;
    }
    if (descriptor->ports.size() != 1 || descriptor->ports.front().name != "count" ||
        descriptor->ports.front().direction != nxd::host::PortDescriptor::Direction::Out) {
        std::cerr << "valid_descriptor_loads: port mismatch\n";
        return false;
    }
    if (descriptor->parameters.size() != 1 || descriptor->parameters.front().name != "limit") {
        std::cerr << "valid_descriptor_loads: parameter mismatch\n";
        return false;
    }
    if (descriptor->thread_roles.size() != 1 || descriptor->thread_roles.front().name != "worker") {
        std::cerr << "valid_descriptor_loads: thread role mismatch\n";
        return false;
    }
    if (descriptor->resources.max_heap_bytes != 65536 || !descriptor->resources.deterministic) {
        std::cerr << "valid_descriptor_loads: resources mismatch\n";
        return false;
    }
    if (descriptor->supported_modes.size() != 1 || descriptor->supported_modes.front() != "default") {
        std::cerr << "valid_descriptor_loads: supported modes mismatch\n";
        return false;
    }
    return true;
}

bool missing_name_rejected() {
    const auto path = write_temp("nxd_missing_name.json", R"({
        "version": "1.0.0",
        "sdk": {"abi_major": 1, "abi_minor": 0},
        "contracts": [],
        "ports": [],
        "parameters": [],
        "thread_roles": [],
        "resources": {},
        "supported_modes": []
    })");
    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (descriptor) {
        std::cerr << "missing_name_rejected: should have failed\n";
        return false;
    }
    return true;
}

bool invalid_port_direction_rejected() {
    const auto path = write_temp("nxd_bad_direction.json", R"({
        "name": "test",
        "version": "1.0.0",
        "sdk": {"abi_major": 1, "abi_minor": 0},
        "contracts": [],
        "ports": [
            {"name": "p", "direction": "sideways", "semantics": "sample", "contract": "C", "contract_version": "1.0.0"}
        ],
        "parameters": [],
        "thread_roles": [],
        "resources": {},
        "supported_modes": []
    })");
    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (descriptor) {
        std::cerr << "invalid_port_direction_rejected: should have failed\n";
        return false;
    }
    return true;
}

bool invalid_parameter_type_rejected() {
    const auto path = write_temp("nxd_bad_param.json", R"({
        "name": "test",
        "version": "1.0.0",
        "sdk": {"abi_major": 1, "abi_minor": 0},
        "contracts": [],
        "ports": [],
        "parameters": [
            {"name": "limit", "type": 123}
        ],
        "thread_roles": [],
        "resources": {},
        "supported_modes": []
    })");
    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (descriptor) {
        std::cerr << "invalid_parameter_type_rejected: should have failed\n";
        return false;
    }
    return true;
}

bool missing_supported_modes_rejected() {
    const auto path = write_temp("nxd_missing_modes.json", R"({
        "name": "test",
        "version": "1.0.0",
        "sdk": {"abi_major": 1, "abi_minor": 0},
        "contracts": [],
        "ports": [],
        "parameters": [],
        "thread_roles": [],
        "resources": {}
    })");
    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (descriptor) {
        std::cerr << "missing_supported_modes_rejected: should have failed\n";
        return false;
    }
    return true;
}

bool invalid_json_rejected() {
    const auto path = write_temp("nxd_invalid_json.json", "not json");
    const auto descriptor = nxd::host::load_component_descriptor(path);
    std::filesystem::remove(path);
    if (descriptor) {
        std::cerr << "invalid_json_rejected: should have failed\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!valid_descriptor_loads()) return 1;
    if (!missing_name_rejected()) return 2;
    if (!invalid_port_direction_rejected()) return 3;
    if (!invalid_parameter_type_rejected()) return 4;
    if (!missing_supported_modes_rejected()) return 5;
    if (!invalid_json_rejected()) return 6;

    std::cout << "ok\n";
    return 0;
}
