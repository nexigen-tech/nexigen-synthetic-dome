/* NXD Chronos — frame-context distribution to a partition.
 *
 * A partition owns a lock-free sequence lock that publishes the most recent
 * frame context.  Writing and reading perform no heap allocation and use only
 * atomic operations so that all components in the partition observe identical
 * values for the same frame.
 */

#ifndef NXD_RUNTIME_CHRONOS_PARTITION_HPP
#define NXD_RUNTIME_CHRONOS_PARTITION_HPP

#include "nxd/runtime/chronos/time.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <type_traits>

namespace nxd::runtime::chronos {

/// Frame context distributed to every component in a partition.
struct FrameContext final {
    std::uint64_t frame_no{0};
    SimTime sim_time{0};
    WallTime wall_time{0};
    HwFpgaTime hw_fpga_time{0};
    bool has_hw_fpga_time{false};
};

static_assert(std::is_trivially_copyable_v<FrameContext>,
              "FrameContext must be trivially copyable for lock-free exchange");

/// A partition receives one frame context per base frame.
class Partition final {
  public:
    Partition(std::uint32_t id, std::string name) noexcept;

    Partition(const Partition&) = delete;
    Partition& operator=(const Partition&) = delete;

    /// Identifier used in the Dome Plan and diagnostics.
    [[nodiscard]] std::uint32_t id() const noexcept;

    /// Human-readable partition name.
    [[nodiscard]] const std::string& name() const noexcept;

    /// Publishes a new frame context to the partition.
    void publish(const FrameContext& ctx) noexcept;

    /// Returns the most recently published frame context.
    [[nodiscard]] FrameContext latest() const noexcept;
  private:
    std::uint32_t id_;
    std::string name_;
    FrameContext snapshot_{};
    std::atomic<std::uint64_t> sequence_{0};
};

} // namespace nxd::runtime::chronos

#endif // NXD_RUNTIME_CHRONOS_PARTITION_HPP
