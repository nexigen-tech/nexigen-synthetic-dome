/* NXD Chronos — simulation-time runtime state machine.
 *
 * Exposes cold-path state transitions (initialize, arm, play, pause, step, stop,
 * dispose) that move a Chronos instance through its lifecycle.  Transitions are
 * performed with a single atomic compare-and-swap so that a rejected request
 * never mutates runtime state, and concurrent transitions observe a consistent
 * ordering.
 */

#ifndef NXD_RUNTIME_CHRONOS_STATE_MACHINE_HPP
#define NXD_RUNTIME_CHRONOS_STATE_MACHINE_HPP

#include "nxd/runtime/chronos/partition.hpp"
#include "nxd/runtime/chronos/time.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <nexigen/core/result.hpp>

namespace nxd::runtime::chronos {

/// Execution mode selected before arming.
enum class Mode : std::uint8_t {
    RealTimeHil = 0, ///< RT/HIL with hardware-time base.
    ScaledSil = 1,   ///< Scaled software-in-the-loop.
    FreeSil = 2,     ///< Free-running software-in-the-loop.
    Step = 3,        ///< Manual one-frame stepping.
};

/// Runtime configuration validated during initialization.
struct Config final {
    Mode mode = Mode::Step;
    std::int64_t base_frame_interval_ns = default_base_frame_interval_ns;
    std::uint32_t scale_factor = 1;
};

/// Lifecycle state of a Chronos runtime instance.
enum class State : std::uint8_t {
    Uninitialized = 0,
    Initialized = 1,
    Armed = 2,
    Playing = 3,
    Paused = 4,
    Stepping = 5,
    Stopped = 6,
    Disposed = 7,
};

/// Error domain returned by Chronos state transitions.
enum class Error : std::uint8_t {
    None = 0,
    InvalidState,
    InvalidMode,
    InvalidConfiguration,
};

/// Deterministic simulation-time runtime.
class Runtime final {
  public:
    Runtime() noexcept = default;
    explicit Runtime(Config cfg) noexcept;

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    /// Configures the runtime and moves from Uninitialized to Initialized.
    [[nodiscard]] nexigen::core::Status initialize() noexcept;

    /// Registers a partition before the runtime is armed. Rejected after arming.
    [[nodiscard]] nexigen::core::Result<Partition*>
    register_partition(std::uint32_t id, std::string name) noexcept;

    /// Prepares the timeline and moves from Initialized to Armed.
    [[nodiscard]] nexigen::core::Status arm() noexcept;

    /// Starts continuous execution. Allowed from Armed or Paused, but not in
    /// Step mode.
    [[nodiscard]] nexigen::core::Status play() noexcept;

    /// Suspists continuous execution. Allowed from Playing only.
    [[nodiscard]] nexigen::core::Status pause() noexcept;

    /// Advances exactly one base frame. Allowed only in Step mode from Armed.
    [[nodiscard]] nexigen::core::Status step() noexcept;

    /// Halts execution and moves to Stopped.
    [[nodiscard]] nexigen::core::Status stop() noexcept;

    /// Tears down resources and moves to Disposed.
    [[nodiscard]] nexigen::core::Status dispose() noexcept;

    /// Current lifecycle state.
    [[nodiscard]] State state() const noexcept;

    /// Selected execution mode.
    [[nodiscard]] Mode mode() const noexcept;

    /// Current frame number.
    [[nodiscard]] std::uint64_t frame_no() const noexcept;

    /// Current logical simulation time.
    [[nodiscard]] SimTime sim_time() const noexcept;

    /// Current wall-clock observation.
    [[nodiscard]] WallTime wall_time() const noexcept;

    /// Current hardware/FPGA time.
    [[nodiscard]] HwFpgaTime hw_fpga_time() const noexcept;
  private:
    Config cfg_{};
    std::atomic<State> state_{State::Uninitialized};
    std::atomic<std::uint64_t> frame_no_{0};
    SimTime sim_time_{0};
    WallTime wall_time_{0};
    HwFpgaTime hw_fpga_time_{0};
    std::vector<std::unique_ptr<Partition>> partitions_;

    void publish_current_frame_context() noexcept;

    [[nodiscard]] nexigen::core::Status
    try_transition(State expected, State next) noexcept;

    [[nodiscard]] static bool is_valid(const Config& cfg) noexcept;
};

/// Converts an Error enumerator to the shared ErrorCode vocabulary.
[[nodiscard]] nexigen::core::ErrorCode to_error_code(Error err) noexcept;

} // namespace nxd::runtime::chronos

#endif // NXD_RUNTIME_CHRONOS_STATE_MACHINE_HPP
