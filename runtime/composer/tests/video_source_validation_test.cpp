/* Composer source-name validation for video streams.
 *
 * Verifies that video logical sources can be declared on partitions and that
 * cross-source wires are rejected.
 */

#include "nxd/runtime/composer/plan.hpp"
#include "nxd/runtime/composer/validation.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace composer = nxd::runtime::composer;

namespace {

[[nodiscard]] std::string read_fixture(const char* name) {
    std::ifstream file(name);
    if (!file) {
        std::cerr << "cannot open fixture " << name << "\n";
        std::exit(1);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <video_camera_front_left.json> <video_isolation.json>\n";
        return 1;
    }

    // Valid Camera_Front_Left-only plan passes all wiring rules.
    {
        const auto plan = composer::DomePlan::from_json(read_fixture(argv[1]));
        if (!plan.has_value()) {
            std::cerr << "failed to parse Camera_Front_Left fixture\n";
            return 10;
        }
        if (!composer::validate_intra_partition_direct_wiring(*plan).empty()) {
            std::cerr << "Camera_Front_Left fixture failed intra-partition validation\n";
            return 11;
        }
        if (!composer::validate_let_boundaries(*plan).empty()) {
            std::cerr << "Camera_Front_Left fixture failed LET-boundary validation\n";
            return 12;
        }
        if (!composer::validate_source_name_wiring(*plan).empty()) {
            std::cerr << "Camera_Front_Left fixture failed source-name validation\n";
            return 13;
        }
        if (plan->partitions.empty() || plan->partitions[0].source_name != "Camera_Front_Left") {
            std::cerr << "Camera_Front_Left fixture missing source name\n";
            return 14;
        }
    }

    // Isolation fixture has one cross-source wire that must be rejected.
    {
        const auto plan = composer::DomePlan::from_json(read_fixture(argv[2]));
        if (!plan.has_value()) {
            std::cerr << "failed to parse isolation fixture\n";
            return 20;
        }
        const auto diagnostics = composer::validate_source_name_wiring(*plan);
        if (diagnostics.size() != 1) {
            std::cerr << "expected one source-name diagnostic, got " << diagnostics.size() << "\n";
            return 21;
        }
        if (diagnostics[0].source_instance != "virtual_video_b" ||
            diagnostics[0].target_instance != "receiver_a") {
            std::cerr << "source-name diagnostic points to unexpected wire\n";
            return 22;
        }
    }

    return 0;
}
