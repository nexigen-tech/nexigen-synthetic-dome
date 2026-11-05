/* Chronos C API table test.
 *
 * Verifies that the exported API table is non-null, has the expected version,
 * and that its function pointers can initialize, arm, step, and stop a runtime
 * instance through the C boundary.
 */

#include "nxd/runtime/chronos/c_api.hpp"

#include <cstdint>

int main() {
    const NxdChronosApiTable* table = nxd_chronos_api();
    if (table == nullptr) return 1;
    if (table->version_major != NXD_CHRONOS_API_MAJOR) return 2;
    if (table->version_minor != NXD_CHRONOS_API_MINOR) return 3;
    if (table->table_size != sizeof(NxdChronosApiTable)) return 4;
    if (table->create == nullptr || table->destroy == nullptr ||
        table->initialize == nullptr || table->arm == nullptr ||
        table->step == nullptr || table->stop == nullptr ||
        table->dispose == nullptr) {
        return 5;
    }

    constexpr std::int64_t base_ns = 1'000'000;
    const NxdChronosConfig config = {
        .mode = NXD_CHRONOS_MODE_STEP,
        .base_frame_interval_ns = base_ns,
        .scale_factor = 1,
    };

    NxdChronosInstance* instance = table->create(&config);
    if (instance == nullptr) return 6;

    if (table->initialize(instance) != NXD_CHRONOS_OK) return 7;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_INITIALIZED) return 8;

    if (table->arm(instance) != NXD_CHRONOS_OK) return 9;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_ARMED) return 10;

    if (table->step(instance) != NXD_CHRONOS_OK) return 11;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_ARMED) return 12;
    if (table->get_frame_no(instance) != 1) return 13;
    if (table->get_sim_time_ns(instance) != base_ns) return 14;

    if (table->stop(instance) != NXD_CHRONOS_OK) return 15;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_STOPPED) return 16;

    if (table->dispose(instance) != NXD_CHRONOS_OK) return 17;
    if (table->get_state(instance) != NXD_CHRONOS_STATE_DISPOSED) return 18;

    table->destroy(instance);
    return 0;
}
