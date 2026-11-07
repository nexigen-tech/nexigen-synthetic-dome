/* Loads the Rust and C++ counter example plugins via dlopen and verifies
 * that, when driven with the same frame context and RNG sequence, both
 * produce the same offset values.
 */

#include "nxd/abi.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <vector>

namespace {

using EntryFn = const NxdAbiTable* (*)();

struct Plugin {
    void* handle = nullptr;
    const NxdAbiTable* table = nullptr;
    NxdComponentState* state = nullptr;
    std::uint64_t seed = 0x123456789ABCDEF0ULL;

    bool load(const char* path) {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::cerr << "dlopen failed: " << dlerror() << "\n";
            return false;
        }
        auto* entry = reinterpret_cast<EntryFn>(dlsym(handle, "nxd_abi_entry"));
        if (!entry) {
            std::cerr << "dlsym failed: " << dlerror() << "\n";
            return false;
        }
        table = entry();
        return table != nullptr;
    }

    bool validate_abi() const {
        return table->abi_version.major == 1 && table->table_size == sizeof(NxdAbiTable) && table->create != nullptr && table->destroy != nullptr && table->arm != nullptr && table->step != nullptr && table->stop != nullptr;
    }

    bool create_state() {
        state = table->create();
        return state != nullptr;
    }

    static std::uint64_t rng_callback(void* self) {
        auto* plugin = static_cast<Plugin*>(self);
        plugin->seed ^= plugin->seed >> 12;
        plugin->seed ^= plugin->seed << 25;
        plugin->seed ^= plugin->seed >> 27;
        return plugin->seed * 0x2545F4914F6CDD1DULL;
    }

    NxdStatus arm(std::uint64_t frame_no) {
        NxdFrameContext ctx{};
        ctx.frame_no = frame_no;
        ctx.sim_time_ns = static_cast<std::int64_t>(frame_no * 1'000'000);
        ctx.hw_time_ns = static_cast<std::int64_t>(frame_no * 1'000);
        ctx.seed_version = 1;
        ctx.rng = rng_callback;
        ctx.rng_state = this;
        return table->arm(state, &ctx);
    }

    NxdStatus step(std::uint64_t frame_no) {
        NxdFrameContext ctx{};
        ctx.frame_no = frame_no;
        ctx.sim_time_ns = static_cast<std::int64_t>(frame_no * 1'000'000);
        ctx.hw_time_ns = static_cast<std::int64_t>(frame_no * 1'000);
        ctx.seed_version = 1;
        ctx.rng = rng_callback;
        ctx.rng_state = this;
        return table->step(state, &ctx);
    }

    void stop() { table->stop(state); }
    void destroy() { table->destroy(state); }
};

std::uint64_t read_offset(NxdComponentState* state) {
    // Both plugins store the same layout at the start of their state.
    auto* p = reinterpret_cast<std::uint64_t*>(state);
    return *p;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <cpp-plugin.so> <rust-plugin.so>\n";
        return 1;
    }

    Plugin cpp;
    Plugin rust;

    if (!cpp.load(argv[1])) return 2;
    if (!rust.load(argv[2])) return 3;

    if (!cpp.validate_abi()) {
        std::cerr << "C++ plugin failed ABI validation\n";
        return 4;
    }
    if (!rust.validate_abi()) {
        std::cerr << "Rust plugin failed ABI validation\n";
        return 5;
    }

    cpp.create_state();
    rust.create_state();

    cpp.arm(0);
    rust.arm(0);

    if (read_offset(cpp.state) != read_offset(rust.state)) {
        std::cerr << "offset mismatch after arm\n";
        return 6;
    }

    for (std::uint64_t frame = 1; frame <= 10; ++frame) {
        cpp.step(frame);
        rust.step(frame);
        if (read_offset(cpp.state) != read_offset(rust.state)) {
            std::cerr << "offset mismatch at frame " << frame << "\n";
            return 7;
        }
    }

    cpp.stop();
    rust.stop();
    cpp.destroy();
    rust.destroy();

    std::cout << "ok\n";
    return 0;
}
