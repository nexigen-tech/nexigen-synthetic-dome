/* NXD walking-skeleton runner (`nexigen-run`).
 *
 * Loads a Dome Plan, validates local executability, launches the declared
 * processes, drives Chronos through one frame, and shuts everything down
 * cleanly.  When configured, it also starts the Recorder, Replay, and
 * Validation services and ties them to the same transport and Chronos
 * timeline.
 */

#include "nxd/contracts/ReplayControl.hpp"
#include "nxd/contracts/ValidationResult.hpp"
#include "nxd/runtime/chronos/c_api.hpp"
#include "nxd/runtime/composer/plan.hpp"
#include "nxd/runtime/recording/recorder.hpp"
#include "nxd/runtime/recording/replay.hpp"
#include "nxd/runtime/transport/headless_transport.hpp"
#include "nxd/runtime/validation/validation.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

namespace composer = nxd::runtime::composer;
namespace recording = nxd::runtime::recording;
namespace transport = nxd::runtime::transport;
namespace validation = nxd::runtime::validation;

[[nodiscard]] validation::PredicateOp parse_validation_op(std::string_view op) noexcept {
    if (op == "less_than") return validation::PredicateOp::LessThan;
    if (op == "less_or_equal") return validation::PredicateOp::LessOrEqual;
    if (op == "greater_than") return validation::PredicateOp::GreaterThan;
    if (op == "greater_or_equal") return validation::PredicateOp::GreaterOrEqual;
    if (op == "equal") return validation::PredicateOp::Equal;
    if (op == "not_equal") return validation::PredicateOp::NotEqual;
    if (op == "present") return validation::PredicateOp::Present;
    return validation::PredicateOp::LessThan;
}

[[nodiscard]] validation::FieldSpec to_validation_field(
    const composer::PlanValidationFieldSpec& f) noexcept {
    validation::FieldSpec spec;
    spec.path = f.path;
    spec.offset = f.offset;
    spec.size = f.size;
    spec.is_floating = f.is_floating;
    spec.is_signed = f.is_signed;
    return spec;
}

[[nodiscard]] bool setup_services(
    const composer::DomePlan& plan,
    transport::HeadlessTransport& transport,
    std::unique_ptr<recording::Recorder>& recorder,
    std::unique_ptr<recording::ReplaySource>& replay,
    std::unique_ptr<validation::ValidationService>& validation_service) noexcept {
    if (plan.recording.enabled) {
        recorder = std::make_unique<recording::Recorder>(transport, plan.recording.output_path);
        for (const auto& topic : plan.recording.topics) {
            if (!recorder->register_topic(topic.source_name,
                                          topic.element_size,
                                          topic.max_count,
                                          topic.layout_hash,
                                          topic.schema_name)) {
                std::cerr << "error: recorder failed to register topic "
                          << topic.source_name << "\n";
                return false;
            }
        }
        if (!recorder->start()) {
            std::cerr << "error: recorder failed to start\n";
            return false;
        }
    }

    if (plan.replay.enabled) {
        replay = std::make_unique<recording::ReplaySource>(transport, plan.replay.input_path);
        if (!replay->load()) {
            std::cerr << "error: replay failed to load " << plan.replay.input_path << "\n";
            return false;
        }
        if (plan.replay.autostart) {
            nxd::contracts::ReplayControl start_cmd{};
            start_cmd.command = nxd::contracts::Command::Start;
            replay->control(start_cmd);

            if (plan.replay.loop) {
                nxd::contracts::ReplayControl loop_cmd{};
                loop_cmd.command = nxd::contracts::Command::SetLoop;
                loop_cmd.loop_enabled = 1;
                replay->control(loop_cmd);
            }

            if (plan.replay.speed != 1.0) {
                nxd::contracts::ReplayControl speed_cmd{};
                speed_cmd.command = nxd::contracts::Command::SetSpeed;
                speed_cmd.speed = plan.replay.speed;
                replay->control(speed_cmd);
            }
        }
    }

    if (plan.validation.enabled) {
        validation_service = std::make_unique<validation::ValidationService>(transport);
        for (const auto& rule : plan.validation.rules) {
            validation::Rule vr;
            vr.name = rule.name;
            vr.topic = rule.topic;
            vr.blackboard_key = rule.blackboard_key;
            vr.op = parse_validation_op(rule.op);
            vr.field = to_validation_field(rule.field);
            vr.reference_field = to_validation_field(rule.reference_field);
            vr.constant = rule.constant;
            vr.has_constant = rule.has_constant;
            vr.constant_string = rule.constant_string;
            vr.is_blackboard_rule = rule.is_blackboard_rule;
            vr.reason_template = rule.reason_template;

            const bool ok = rule.is_blackboard_rule
                                ? validation_service->add_blackboard_rule(vr)
                                : validation_service->add_rule(
                                      vr, rule.element_size, rule.max_count, rule.layout_hash);
            if (!ok) {
                std::cerr << "error: validation failed to add rule " << rule.name << "\n";
                return false;
            }
        }
    }

    return true;
}

[[nodiscard]] bool drive_chronos(
    const composer::PlanChronosConfig& config,
    recording::Recorder* recorder,
    recording::ReplaySource* replay,
    validation::ValidationService* validation_service) noexcept {
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
            return false;
        }
        return true;
    };

    bool ok = true;
    ok = ok && check(api->initialize(instance), "initialize");
    ok = ok && check(api->arm(instance), "arm");
    ok = ok && check(api->step(instance), "step");

    if (ok) {
        const auto frame_no = api->get_frame_no(instance);
        const auto sim_time_ns = api->get_sim_time_ns(instance);

        if (replay != nullptr) {
            (void)replay->step(sim_time_ns);
        }
        if (recorder != nullptr) {
            recorder->poll();
            recorder->record_blackboard();
        }
        if (validation_service != nullptr) {
            validation_service->poll();
        }

        std::cout << "nexigen-run: completed frame " << frame_no
                  << " at sim_time_ns=" << sim_time_ns << "\n";
    }

    // Run the Chronos shutdown sequence even if the frame failed so that
    // resources are released deterministically.
    (void)check(api->stop(instance), "stop");
    ok = check(api->dispose(instance), "dispose") && ok;
    api->destroy(instance);
    return ok;
}

[[nodiscard]] bool teardown_services(
    recording::Recorder* recorder,
    recording::ReplaySource* replay,
    validation::ValidationService* validation_service) noexcept {
    bool ok = true;

    if (validation_service != nullptr) {
        const auto result = validation_service->finish();
        std::cout << "validation: outcome=" << static_cast<int>(result.outcome)
                  << " rules=" << result.rule_count
                  << " passes=" << result.pass_count
                  << " fails=" << result.fail_count
                  << " failure_count=" << result.failure_count << "\n";
    }

    if (recorder != nullptr) {
        if (!recorder->stop()) {
            std::cerr << "error: recorder failed to stop cleanly\n";
            ok = false;
        }
    }

    if (replay != nullptr) {
        nxd::contracts::ReplayControl stop_cmd{};
        stop_cmd.command = nxd::contracts::Command::Stop;
        replay->control(stop_cmd);
    }

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

    auto plan_opt = composer::DomePlan::from_json(*plan_text_opt);
    if (!plan_opt.has_value()) {
        std::cerr << "error: failed to parse Dome Plan from " << plan_path << "\n";
        return EXIT_FAILURE;
    }
    const auto& plan = *plan_opt;

    if (!validate_executables(plan.processes, plan_dir)) {
        return EXIT_FAILURE;
    }

    const bool services_active =
        plan.recording.enabled || plan.replay.enabled || plan.validation.enabled;
    std::unique_ptr<transport::HeadlessTransport> transport;
    if (services_active) {
        transport = std::make_unique<transport::HeadlessTransport>();
    }

    std::unique_ptr<recording::Recorder> recorder;
    std::unique_ptr<recording::ReplaySource> replay;
    std::unique_ptr<validation::ValidationService> validation_service;
    if (services_active) {
        if (!setup_services(plan, *transport, recorder, replay, validation_service)) {
            return EXIT_FAILURE;
        }
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

    bool chronos_ok = drive_chronos(plan.chronos,
                                    recorder.get(),
                                    replay.get(),
                                    validation_service.get());

    if (!teardown_services(recorder.get(), replay.get(), validation_service.get())) {
        chronos_ok = false;
    }

    terminate_children(spawned);

    if (!chronos_ok) {
        return EXIT_FAILURE;
    }

    std::cout << "nexigen-run: walking skeleton finished cleanly\n";
    return EXIT_SUCCESS;
}
