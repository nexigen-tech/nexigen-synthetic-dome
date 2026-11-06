#pragma once

#include <cstdint>

#include "nxd/abi.hpp"
#include "nxd/host/component.hpp"

namespace nxd::host {

    /// Outcome of a headless frame run.
    struct DriverResult {
        std::uint64_t frames_completed = 0;
        NxdStatus last_status = NXD_OK;
        std::uint64_t failed_frame = 0;
        bool stopped_normally = false;
    };

    /// Caller-driven manual driver.  The driver fills frame contexts with
    /// deterministic RNG and monotonic time, arms the component, then steps it for
    /// the requested number of frames.  A step failure records the numeric status
    /// and frame number without formatting and enters the normal stop path.
    class HeadlessDriver {
      public:
        explicit HeadlessDriver(ComponentInstance& component) noexcept;

        [[nodiscard]] DriverResult run(std::uint64_t frame_count, std::uint64_t seed,
                                       std::uint64_t sim_time_step_ns,
                                       std::uint64_t hw_time_step_ns,
                                       std::uint64_t start_frame = 0) noexcept;

      private:
        ComponentInstance* component_;
    };

} // namespace nxd::host
