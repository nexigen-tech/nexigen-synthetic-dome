/* NXD walking-skeleton runner (`nexigen-run`).
 *
 * Loads a Dome Plan, validates local executability, launches the declared
 * processes, drives Chronos through one frame, and shuts everything down
 * cleanly.
 */

#include "nxd/runtime/chronos/c_api.hpp"
#include "nxd/runtime/composer/plan.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path) noexcept {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "error: cannot open plan file: " << path << "\n";
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!file && !file.eof()) {
        std::cerr << "error: cannot read plan file: " << path << "\n";
        return std::nullopt;
    }
    return buffer.str();
}

[[nodiscard]] bool validate_executables(
    const std::span<const nxd::runtime::composer::Process> processes,
    const std::filesystem::path& plan_dir) noexcept {
    for (const auto& proc : processes) {
        std::filesystem::path exe = proc.executable;
        if (exe.is_relative()) {
            exe = plan_dir / exe;
        }
        std::error_code ec;
        const auto status = std::filesystem::symlink_status(exe, ec);
        if (ec || !std::filesystem::exists(status)) {
            std::cerr << "error: process '" << proc.id << "' executable not found: "
                      << exe << "\n";
            return false;
        }
        const auto perms = std::filesystem::status(exe, ec).permissions();
        if (ec) {
            std::cerr << "error: cannot inspect permissions for: " << exe << "\n";
            return false;
        }
        if ((perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::none &&
            (perms & std::filesystem::perms::group_exec) == std::filesystem::perms::none &&
            (perms & std::filesystem::perms::others_exec) == std::filesystem::perms::none) {
            std::cerr << "error: process '" << proc.id << "' executable is not executable: "
                      << exe << "\n";
            return false;
        }
    }
    return true;
}

struct SpawnedProcess final {
    std::string id;
    pid_t pid = -1;
};

[[nodiscard]] bool spawn_processes(
    const std::span<const nxd::runtime::composer::Process> processes,
    const std::filesystem::path& plan_dir,
    std::vector<SpawnedProcess>& spawned) noexcept {
    for (const auto& proc : processes) {
        std::filesystem::path exe = proc.executable;
        if (exe.is_relative()) {
            exe = plan_dir / exe;
        }
        const std::string exe_str = exe.string();

        const char* argv[] = {exe_str.c_str(), nullptr};

        posix_spawn_file_actions_t actions{};
        posix_spawn_file_actions_init(&actions);
        // Do not leak this runner's stdin/stdout/stderr handles into children.
        // Keep stdout/stderr so component logs are visible, but detach stdin.
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);

        posix_spawnattr_t attr{};
        posix_spawnattr_init(&attr);
        // Reset signal mask and dispositions so children start clean.
        sigset_t default_mask;
        sigemptyset(&default_mask);
        posix_spawnattr_setsigmask(&attr, &default_mask);
        short flags = POSIX_SPAWN_SETSIGMASK;
        posix_spawnattr_setflags(&attr, flags);

        pid_t pid = -1;
        const int rc = posix_spawn(&pid, exe_str.c_str(), &actions, &attr,
                                     const_cast<char* const*>(argv), environ);

        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&actions);

        if (rc != 0) {
            std::cerr << "error: failed to spawn '" << proc.id << "': "
                      << std::strerror(rc) << "\n";
            return false;
        }

        spawned.push_back(SpawnedProcess{proc.id, pid});
    }
    return true;
}

void terminate_children(std::vector<SpawnedProcess>& spawned) noexcept {
    // Graceful shutdown: SIGTERM, then wait.
    for (const auto& child : spawned) {
        if (child.pid > 0) {
            kill(child.pid, SIGTERM);
        }
    }
    for (const auto& child : spawned) {
        if (child.pid > 0) {
            int status = 0;
            waitpid(child.pid, &status, 0);
        }
    }
}

[[nodiscard]] bool drive_chronos(
    const nxd::runtime::composer::PlanChronosConfig& config) noexcept {
    const auto* api = nxd_chronos_api();
    if (api == nullptr) {
        std::cerr << "error: cannot load Chronos C API table\n";
        return false;
    }

    NxdChronosConfig chronos_config{};
    chronos_config.mode = config.mode;
    chronos_config.base_frame_interval_ns = config.base_frame_interval_ns;
    chronos_config.scale_factor = config.scale_factor;

    auto* instance = api->create(&chronos_config);
    if (instance == nullptr) {
        std::cerr << "error: Chronos create failed\n";
        return false;
    }

    auto check = [&](NxdChronosStatus status, const char* op) -> bool {
        if (status != NXD_CHRONOS_OK) {
            std::cerr << "error: Chronos " << op << " failed with status " << status << "\n";
            api->destroy(instance);
            return false;
        }
        return true;
    };

    bool ok = true;
    ok = ok && check(api->initialize(instance), "initialize");
    ok = ok && check(api->arm(instance), "arm");
    ok = ok && check(api->step(instance), "step");
    ok = ok && check(api->stop(instance), "stop");

    if (ok) {
        const auto frame_no = api->get_frame_no(instance);
        const auto sim_time_ns = api->get_sim_time_ns(instance);
        std::cout << "nexigen-run: completed frame " << frame_no
                  << " at sim_time_ns=" << sim_time_ns << "\n";
    }

    check(api->dispose(instance), "dispose");
    api->destroy(instance);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: nexigen-run <plan.json>\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path plan_path(argv[1]);
    const auto plan_dir = plan_path.parent_path();

    const auto plan_text_opt = read_file(plan_path);
    if (!plan_text_opt.has_value()) {
        return EXIT_FAILURE;
    }

    auto plan_opt = nxd::runtime::composer::DomePlan::from_json(*plan_text_opt);
    if (!plan_opt.has_value()) {
        std::cerr << "error: failed to parse Dome Plan from " << plan_path << "\n";
        return EXIT_FAILURE;
    }
    const auto& plan = *plan_opt;

    if (!validate_executables(plan.processes, plan_dir)) {
        return EXIT_FAILURE;
    }

    std::vector<SpawnedProcess> spawned;
    if (!spawn_processes(plan.processes, plan_dir, spawned)) {
        terminate_children(spawned);
        return EXIT_FAILURE;
    }

    // Allow child processes a moment to initialize before distributing the first
    // frame.  Real runner would wait for a readiness signal; this skeleton uses
    // a short sleep.
    usleep(50'000);

    bool chronos_ok = drive_chronos(plan.chronos);

    terminate_children(spawned);

    if (!chronos_ok) {
        return EXIT_FAILURE;
    }

    std::cout << "nexigen-run: walking skeleton finished cleanly\n";
    return EXIT_SUCCESS;
}
