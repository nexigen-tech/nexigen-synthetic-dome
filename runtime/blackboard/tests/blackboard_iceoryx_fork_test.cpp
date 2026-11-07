#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr std::array<std::uint8_t, 32> layout_hash = {
    0x5b, 0x1f, 0xfb, 0xd5, 0x0f, 0x10, 0xca, 0x2b, 0xfe, 0x34, 0xb7, 0xb3, 0x29, 0x85, 0xa1, 0xeb,
    0xf0, 0x7f, 0x70, 0x5a, 0x20, 0x1a, 0x95, 0x3b, 0x1a, 0x5d, 0xca, 0x4c, 0xa6, 0x4b, 0x84, 0x35};

constexpr std::array<std::uint8_t, 32> bad_hash = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct alignas(8) State {
    std::uint64_t value = 0;
};

} // namespace

int main() {
    if (setenv("NXD_BLACKBOARD_BACKEND", "iceoryx2", 1) != 0) {
        std::cerr << "failed to set backend env\n";
        return 1;
    }

    nxd_blackboard_initialize();

    const std::string base_name = "bb_iceoryx_fork/" + std::to_string(getpid());
    const std::string entry_name = base_name + "/state";

    NxdBlackboardEntry* writer = nxd_blackboard_create_entry(
        entry_name.c_str(), sizeof(State), layout_hash.data());
    if (writer == nullptr) {
        std::cerr << "parent: failed to create entry\n";
        return 1;
    }

    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "parent: fork failed\n";
        return 1;
    }

    if (child == 0) {
        // Child process: open the entry for reading and wait for a value.
        nxd_blackboard_initialize();

        // Attempting to create the same entry from the panel process must fail
        // because the parent is already the writer.
        if (nxd_blackboard_create_entry(entry_name.c_str(), sizeof(State), layout_hash.data()) != nullptr) {
            std::cerr << "child: second writer unexpectedly succeeded\n";
            _exit(1);
        }

        // Opening with a mismatched layout hash must fail.
        if (nxd_blackboard_open_entry(entry_name.c_str(), bad_hash.data()) != nullptr) {
            std::cerr << "child: mismatched layout hash unexpectedly accepted\n";
            _exit(1);
        }

        const NxdBlackboardEntry* reader = nxd_blackboard_open_entry(
            entry_name.c_str(), layout_hash.data());
        if (reader == nullptr) {
            std::cerr << "child: failed to open entry\n";
            _exit(1);
        }

        State read_state{};
        std::uint64_t rev = 0;
        std::uint64_t app = 0;
        for (int attempt = 0; attempt < 1000; ++attempt) {
            if (nxd_blackboard_read(reader, &read_state, sizeof(read_state), &rev, &app) == NXD_BLACKBOARD_OK) {
                break;
            }
            usleep(1'000);
        }

        if (read_state.value != 0xDEADBEEFCAFEBABEULL) {
            std::cerr << "child: payload mismatch: got " << read_state.value << "\n";
            _exit(1);
        }
        if (rev != 1 || app != 100) {
            std::cerr << "child: metadata mismatch rev=" << rev << " app=" << app << "\n";
            _exit(1);
        }

        NxdBlackboardUpdate update{};
        if (nxd_blackboard_get_update(reader, &update) != NXD_BLACKBOARD_OK) {
            std::cerr << "child: get_update failed\n";
            _exit(1);
        }
        if (std::memcmp(update.layout_hash, layout_hash.data(), layout_hash.size()) != 0 ||
            update.revision != rev || update.apply_frame_no != app || update.generation != 1) {
            std::cerr << "child: update mismatch\n";
            _exit(1);
        }

        _exit(0);
    }

    // Parent process: publish after the child has had time to subscribe.
    usleep(200'000);

    State write_state{0xDEADBEEFCAFEBABEULL};
    const std::int64_t revision = nxd_blackboard_write(writer, &write_state, sizeof(write_state), 100);
    if (revision < 0) {
        std::cerr << "parent: write failed: " << revision << "\n";
        return 1;
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        std::cerr << "parent: waitpid failed\n";
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "parent: child failed\n";
        return 1;
    }

    return 0;
}
