/* Child-process panic and failure-injection tests.
 *
 * Runs a Rust panic plugin and a C++ step-failure plugin in isolated child
 * processes and verifies that neither continues into a later frame nor corrupts
 * the parent test runner.
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

struct ChildResult {
    bool clean_exit = false;
    bool signalled = false;
    int exit_code = -1;
    int signal = -1;
    std::string output;
};

ChildResult run_child(const char* runner, const char* plugin, const char* descriptor,
                      std::uint64_t frame_count, std::uint64_t seed) {
    int stdout_pipe[2]{};
    if (pipe(stdout_pipe) != 0) {
        std::cerr << "pipe failed\n";
        std::exit(1);
    }

    const pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork failed\n";
        std::exit(1);
    }

    if (pid == 0) {
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);

        const std::string frames = std::to_string(frame_count);
        const std::string seed_str = std::to_string(seed);
        const char* argv[] = {
            runner,
            plugin,
            descriptor,
            frames.c_str(),
            seed_str.c_str(),
            nullptr,
        };
        execvp(runner, const_cast<char* const*>(argv));
        std::cerr << "execvp failed: " << std::strerror(errno) << "\n";
        std::exit(127);
    }

    close(stdout_pipe[1]);
    ChildResult result{};
    char buffer[256]{};
    ssize_t n = 0;
    while ((n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        result.output += buffer;
    }
    close(stdout_pipe[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "waitpid failed\n";
        std::exit(1);
    }

    if (WIFEXITED(status)) {
        result.clean_exit = true;
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signalled = true;
        result.signal = WTERMSIG(status);
    }
    return result;
}

bool rust_panic_aborts(const char* runner, const char* panic_plugin, const char* descriptor) {
    const auto result = run_child(runner, panic_plugin, descriptor, 5, 0);
    if (!result.signalled) {
        std::cerr << "panic test: child should have terminated by signal\n";
        std::cerr << "output: " << result.output << "\n";
        return false;
    }
    if (result.signal != SIGABRT) {
        std::cerr << "panic test: expected SIGABRT, got " << result.signal << "\n";
        return false;
    }
    return true;
}

bool cpp_failure_injection_reports_status(const char* runner, const char* failure_plugin,
                                          const char* descriptor) {
    const auto result = run_child(runner, failure_plugin, descriptor, 10, 0);
    if (!result.clean_exit) {
        std::cerr << "failure injection: child should have exited cleanly\n";
        return false;
    }
    if (result.exit_code != 0) {
        std::cerr << "failure injection: child exited with code " << result.exit_code << "\n";
        return false;
    }
    if (result.output.find("frames_completed=0") == std::string::npos) {
        std::cerr << "failure injection: expected frames_completed=0 in output: "
                  << result.output << "\n";
        return false;
    }
    if (result.output.find("last_status=6") == std::string::npos) {
        std::cerr << "failure injection: expected last_status=6 in output: " << result.output
                  << "\n";
        return false;
    }
    if (result.output.find("stopped_normally=0") == std::string::npos) {
        std::cerr << "failure injection: expected stopped_normally=0 in output: "
                  << result.output << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0]
                  << " <child-runner> <panic-rust.so> <failure-cpp.so> <descriptor.json>\n";
        return 1;
    }

    const char* runner = argv[1];
    const char* panic_plugin = argv[2];
    const char* failure_plugin = argv[3];
    const char* descriptor = argv[4];

    if (!rust_panic_aborts(runner, panic_plugin, descriptor)) {
        return 2;
    }
    if (!cpp_failure_injection_reports_status(runner, failure_plugin, descriptor)) {
        return 3;
    }

    std::cout << "ok\n";
    return 0;
}
