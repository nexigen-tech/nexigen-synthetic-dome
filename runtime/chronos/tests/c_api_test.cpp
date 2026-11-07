/* Chronos C API table test.
 *
 * Verifies that the exported API table is non-null, has the expected version,
 * and that its function pointers can initialize, arm, step, and stop a runtime
 * instance through the C boundary.
 */

#include "nxd/runtime/chronos/c_api.hpp"

#include <cstdint>

static bool test_api_table_is_valid() {
    const NxdChronosApiTable* table = nxd_chronos_api();
    if (table == nullptr) return false;
    if (table->version_major != NXD_CHRONOS_API_MAJOR) return false;
    if (table->version_minor != NXD_CHRONOS_API_MINOR) return false;
    if (table->table_size != sizeof(NxdChronosApiTable)) return false;
    if (table->create == nullptr || table->destroy == nullptr ||
        table->initialize == nullptr || table->arm == nullptr ||
        table->step == nullptr || table->stop == nullptr ||
        table->dispose == nullptr) {
        return false;
    }
    return true;
}

static bool test_c_api_lifecycle() {
    const NxdChronosApiTable* table = nxd_chronos_api();

    constexpr std::int64_t base_ns = 1'000'000;
    const NxdChronosConfig config = {
        .mode = NXD_CHRONOS_MODE_STEP,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    };

    NxdChronosInstance* instance = table->create(&config);
    if (instance == nullptr) return false;

    if (table->initialize(instance) != NXD_CHRONOS_OK) return false;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_INITIALIZED) return false;

    if (table->arm(instance) != NXD_CHRONOS_OK) return false;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_ARMED) return false;

    if (table->step(instance) != NXD_CHRONOS_OK) return false;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_ARMED) return false;
    if (table->get_frame_no(instance) != 1) return false;
    if (table->get_sim_time_ns(instance) != base_ns) return false;

    if (table->stop(instance) != NXD_CHRONOS_OK) return false;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_STOPPED) return false;

    if (table->dispose(instance) != NXD_CHRONOS_OK) return false;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_DISPOSED) return false;

    table->destroy(instance);
    return true;
}

int main() {
    if (!test_api_table_is_valid()) return 1;
    if (!test_c_api_lifecycle()) return 2;

    return 0;
}
