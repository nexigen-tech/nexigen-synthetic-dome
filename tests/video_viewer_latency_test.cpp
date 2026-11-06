/* Automated RX-to-display latency test for the Qt/OpenGL video viewer.
 *
 * Runs the viewer in offscreen mode for a fixed number of frames, captures the
 * latency report printed by the viewer, and verifies that the RX-to-display
 * latency stays within a reasonable bound.
 */

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool parse_latency_line(const std::string& line,
                                      std::uint64_t& count,
                                      std::uint64_t& min_us,
                                      std::uint64_t& max_us,
                                      std::uint64_t& mean_us) noexcept {
    // Expected format: "latency: count=N min_us=N max_us=N mean_us=N"
    const char* prefix = "latency: count=";
    if (line.find(prefix) != 0) {
        return false;
    }
    const char* p = line.c_str() + std::strlen(prefix);

    auto parse_u64 = [](const char*& ptr) -> std::uint64_t {
        std::uint64_t value = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            value = value * 10 + static_cast<std::uint64_t>(*ptr - '0');
            ++ptr;
        }
        return value;
    };

    count = parse_u64(p);
    if (std::strncmp(p, " min_us=", 8) != 0) {
        return false;
    }
    p += 8;
    min_us = parse_u64(p);
    if (std::strncmp(p, " max_us=", 8) != 0) {
        return false;
    }
    p += 8;
    max_us = parse_u64(p);
    if (std::strncmp(p, " mean_us=", 9) != 0) {
        return false;
    }
    p += 9;
    mean_us = parse_u64(p);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0]
                  << " <nxd_video_viewer> <virtual_video.descriptor.json> "
                     "<libnxd_example_virtual_video_cpp.so>\n";
        return 1;
    }

    // Force the offscreen platform so the test runs without a display server.
    if (setenv("QT_QPA_PLATFORM", "offscreen", 1) != 0) {
        std::cerr << "failed to set QT_QPA_PLATFORM\n";
        return 2;
    }

    std::string cmd =
        std::string(argv[1]) + " " + argv[2] + " " + argv[3] + " --headless --frames 10 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        std::cerr << "failed to run viewer\n";
        return 3;
    }

    std::string output;
    std::array<char, 256> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int rc = pclose(pipe);
    if (rc != 0) {
        std::cerr << "viewer exited with code " << rc << "\n" << output << "\n";
        return 4;
    }

    std::uint64_t count = 0;
    std::uint64_t min_us = 0;
    std::uint64_t max_us = 0;
    std::uint64_t mean_us = 0;
    bool found = false;
    std::size_t pos = 0;
    while ((pos = output.find('\n', pos)) != std::string::npos) {
        const std::string line = output.substr(0, pos);
        if (parse_latency_line(line, count, min_us, max_us, mean_us)) {
            found = true;
            break;
        }
        output.erase(0, pos + 1);
        pos = 0;
    }
    if (!found && !output.empty()) {
        // Try the remaining tail without a trailing newline.
        found = parse_latency_line(output, count, min_us, max_us, mean_us);
    }

    if (!found) {
        std::cerr << "latency report not found in viewer output:\n" << output << "\n";
        return 5;
    }

    if (count != 10) {
        std::cerr << "expected 10 latency samples, got " << count << "\n";
        return 6;
    }
    if (min_us == 0 || mean_us == 0) {
        std::cerr << "latency too small: min_us=" << min_us << " mean_us=" << mean_us << "\n";
        return 7;
    }
    if (max_us > 100'000) {
        std::cerr << "latency too high: max_us=" << max_us << "\n";
        return 8;
    }

    std::cout << "video viewer latency: count=" << count << " min_us=" << min_us
              << " max_us=" << max_us << " mean_us=" << mean_us << "\n";
    return 0;
}
