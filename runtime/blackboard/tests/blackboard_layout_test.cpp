#include "nxd/runtime/blackboard/blackboard_c_api.h"
#include "nxd/contracts/ScenarioState.hpp"
#include "nxd/contracts/ScenarioCommand.hpp"
#include "nxd/contracts/BlackboardUpdate.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

constexpr std::array<std::uint8_t, 32> scenario_state_hash = {
    0x5b, 0x1f, 0xfb, 0xd5, 0x0f, 0x10, 0xca, 0x2b, 0xfe, 0x34, 0xb7, 0xb3, 0x29, 0x85, 0xa1, 0xeb,
    0xf0, 0x7f, 0x70, 0x5a, 0x20, 0x1a, 0x95, 0x3b, 0x1a, 0x5d, 0xca, 0x4c, 0xa6, 0x4b, 0x84, 0x35};

} // namespace

int main() {
    static_assert(sizeof(nxd::contracts::ScenarioState) == 8);
    static_assert(alignof(nxd::contracts::ScenarioState) == 4);
    static_assert(sizeof(nxd::contracts::ScenarioCommand) == 24);
    static_assert(alignof(nxd::contracts::ScenarioCommand) == 8);
    static_assert(sizeof(nxd::contracts::BlackboardUpdate) == 120);

    nxd_blackboard_initialize();

    NxdBlackboardEntry* entry = nxd_blackboard_create_entry(
        "scenario/state", sizeof(nxd::contracts::ScenarioState), scenario_state_hash.data());
    if (entry == nullptr) {
        return 1;
    }

    nxd::contracts::ScenarioState state{};
    state.value = 42;
    state.generation = 1;

    std::int64_t revision = nxd_blackboard_write(
        entry, &state, sizeof(state), 7);
    if (revision <= 0) {
        return 2;
    }

    nxd::contracts::ScenarioState read_state{};
    std::uint64_t read_revision = 0;
    std::uint64_t read_apply_frame = 0;
    if (nxd_blackboard_read(entry, &read_state, sizeof(read_state),
                            &read_revision, &read_apply_frame) != NXD_BLACKBOARD_OK) {
        return 3;
    }

    if (read_state.value != 42 || read_state.generation != 1) {
        return 4;
    }
    if (read_revision != static_cast<std::uint64_t>(revision) || read_apply_frame != 7) {
        return 5;
    }

    NxdBlackboardUpdate update{};
    if (nxd_blackboard_get_update(entry, &update) != NXD_BLACKBOARD_OK) {
        return 6;
    }
    if (std::string_view(update.entry_name) != "scenario/state") {
        return 7;
    }
    if (update.revision != static_cast<std::uint64_t>(revision) || update.apply_frame_no != 7) {
        return 8;
    }

    return 0;
}
