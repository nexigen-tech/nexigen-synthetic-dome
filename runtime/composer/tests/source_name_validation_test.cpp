/* Composer source-name isolation tests.
 *
 * Verifies that plan partitions can declare a logical source name and that
 * Composer wiring validation rejects cross-source connections.
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
                  << " <virtual_can_can_a.json> <virtual_can_isolation.json>\n";
        return 1;
    }

    // Valid CAN_A-only plan passes all wiring rules.
    {
        const auto can_a = composer::DomePlan::from_json(read_fixture(argv[1]));
        if (!can_a.has_value()) {
            std::cerr << "failed to parse CAN_A fixture\n";
            return 10;
        }
        if (!composer::validate_intra_partition_direct_wiring(*can_a).empty()) {
            std::cerr << "CAN_A fixture failed intra-partition validation\n";
            return 11;
        }
        if (!composer::validate_let_boundaries(*can_a).empty()) {
            std::cerr << "CAN_A fixture failed LET-boundary validation\n";
            return 12;
        }
        if (!composer::validate_source_name_wiring(*can_a).empty()) {
            std::cerr << "CAN_A fixture failed source-name validation\n";
            return 13;
        }
        if (can_a->partitions.empty() || can_a->partitions[0].source_name != "CAN_A") {
            std::cerr << "CAN_A fixture missing source name\n";
            return 14;
        }
    }

    // Isolation fixture has one cross-source wire that must be rejected.
    {
        const auto isolation = composer::DomePlan::from_json(read_fixture(argv[2]));
        if (!isolation.has_value()) {
            std::cerr << "failed to parse isolation fixture\n";
            return 20;
        }
        const auto diagnostics = composer::validate_source_name_wiring(*isolation);
        if (diagnostics.size() != 1) {
            std::cerr << "expected one source-name diagnostic, got " << diagnostics.size()
                      << "\n";
            return 21;
        }
        if (diagnostics[0].source_instance != "virtual_can_b" ||
            diagnostics[0].target_instance != "monitor_a") {
            std::cerr << "source-name diagnostic points to unexpected wire\n";
            return 22;
        }
    }

    return 0;
}
