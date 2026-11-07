#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>

#include "nxd/host/component.hpp"
#include "nxd/host/headless_driver.hpp"
#include "nxd/host/plugin.hpp"
#include "nxd/host/thread_roles.hpp"
#include <nexigen/core/result.hpp>

namespace nxd::host {

/// High-level headless harness.  Loads a generated descriptor and its plugin,
/// validates the ABI and descriptor hash, then exposes the full lifecycle for
/// caller-driven tests.  All heavy work happens on the cold path; `run()` does
/// no allocation or formatting.
class HeadlessHarness {
  public:
    HeadlessHarness(const std::filesystem::path& plugin_path,
                    const std::filesystem::path& descriptor_path,
                    const HostCapabilities& caps = {}) noexcept;

    /// Load the descriptor and shared library and validate everything before
    /// any lifecycle function is invoked.
    [[nodiscard]] nexigen::core::Status load() noexcept;

    /// Create and initialize the component.
    [[nodiscard]] nexigen::core::Status initialize(std::string_view config = {}) noexcept;

    /// Configures typed port bindings before arming.  The harness owns the
    /// buffers and exposes them via `port_buffer()`.
    void configure_ports(std::vector<PortBinding> bindings) noexcept;

    /// Arm the component with the given deterministic seed.
    [[nodiscard]] nexigen::core::Status arm(std::uint64_t seed) noexcept;

    /// Run the requested number of frames.  The result is stored and also
    /// returned by reference via `last_result()`.
    [[nodiscard]] nexigen::core::Status run(std::uint64_t frame_count,
                                            std::uint64_t sim_time_step_ns = 1'000'000,
                                            std::uint64_t hw_time_step_ns = 1'000) noexcept;

    /// Total number of frames simulated by this harness across all `run()`
    /// calls.  This lets callers advance simulation time incrementally.
    [[nodiscard]] std::uint64_t frames_run() const noexcept;

    [[nodiscard]] const LoadedPlugin* plugin() const noexcept;
    [[nodiscard]] ComponentInstance* component() noexcept;
    [[nodiscard]] const DriverResult& last_result() const noexcept;
    [[nodiscard]] const std::vector<ThreadRoleReport>& thread_reports() const noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept;

    /// Returns a mutable span over the buffer for the named port.  The
    /// component must be configured and not yet destroyed.
    template <typename T>
    [[nodiscard]] std::span<T> port_buffer(const std::string& name) noexcept {
        if (!component_) {
            return {};
        }
        for (auto& binding : component_->port_bindings()) {
            if (binding.name == name) {
                return std::span<T>(reinterpret_cast<T*>(binding.buffer.data()),
                                    binding.buffer.size() / sizeof(T));
            }
        }
        return {};
    }
  private:
    std::filesystem::path plugin_path_;
    std::filesystem::path descriptor_path_;
    HostCapabilities caps_;
    std::optional<LoadedPlugin> plugin_;
    std::optional<ComponentInstance> component_;
    std::vector<PortBinding> bindings_;
    DriverResult last_result_{};
    std::vector<ThreadRoleReport> thread_reports_{};
    std::uint64_t seed_ = 0;
    std::uint64_t frames_run_ = 0;
};

} // namespace nxd::host
