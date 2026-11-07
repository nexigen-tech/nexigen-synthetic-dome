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

struct alignas(8) State {
    std::uint64_t value = 0;
};

bool read_value(const std::string& entry_name, std::uint64_t expected_value, std::uint64_t expected_revision) {
    nxd_blackboard_initialize();

    const NxdBlackboardEntry* reader = nxd_blackboard_open_entry(entry_name.c_str(), layout_hash.data());
    if (reader == nullptr) {
        std::cerr << "child: failed to open entry\n";
        return false;
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

    if (read_state.value != expected_value) {
        std::cerr << "child: payload mismatch: got " << read_state.value << " expected " << expected_value << "\n";
        return false;
    }
    if (rev != expected_revision) {
        std::cerr << "child: revision mismatch: got " << rev << " expected " << expected_revision << "\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (setenv("NXD_BLACKBOARD_BACKEND", "iceoryx2", 1) != 0) {
        std::cerr << "failed to set backend env\n";
        return 1;
    }

    nxd_blackboard_initialize();

    const std::string entry_name = "bb_iceoryx_reconnect/" + std::to_string(getpid());

    NxdBlackboardEntry* writer = nxd_blackboard_create_entry(
        entry_name.c_str(), sizeof(State), layout_hash.data());
    if (writer == nullptr) {
        std::cerr << "parent: failed to create entry\n";
        return 1;
    }

    // First panel process connects, reads the first value, and exits.
    {
        const pid_t child = fork();
        if (child < 0) {
            std::cerr << "parent: fork failed\n";
            return 1;
        }
        if (child == 0) {
            if (!read_value(entry_name, 0x1111'2222'3333'4444ULL, 1)) {
                _exit(1);
            }
            _exit(0);
        }

        usleep(200'000);

        State first{0x1111'2222'3333'4444ULL};
        const std::int64_t rev1 = nxd_blackboard_write(writer, &first, sizeof(first), 10);
        if (rev1 < 0) {
            std::cerr << "parent: first write failed\n";
            return 1;
        }

        int status = 0;
        if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::cerr << "parent: first child failed\n";
            return 1;
        }
    }

    // A second panel process reconnects and observes the next value published
    // after it subscribes.
    {
        const pid_t child = fork();
        if (child < 0) {
            std::cerr << "parent: fork failed\n";
            return 1;
        }
        if (child == 0) {
            if (!read_value(entry_name, 0x5555'6666'7777'8888ULL, 2)) {
                _exit(1);
            }
            _exit(0);
        }

        usleep(200'000);

        State second{0x5555'6666'7777'8888ULL};
        const std::int64_t rev2 = nxd_blackboard_write(writer, &second, sizeof(second), 20);
        if (rev2 < 0) {
            std::cerr << "parent: second write failed\n";
            return 1;
        }

        int status = 0;
        if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::cerr << "parent: second child failed\n";
            return 1;
        }
    }

    return 0;
}
