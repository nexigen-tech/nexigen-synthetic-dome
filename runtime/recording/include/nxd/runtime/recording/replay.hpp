#pragma once

#include "nxd/contracts/ReplayControl.hpp"
#include "nxd/runtime/recording/mcap.hpp"
#include "nxd/runtime/transport/transport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nxd::runtime::recording {

/// Replays a previously recorded MCAP file as a transport source.
class ReplaySource {
  public:
    ReplaySource(transport::Transport& transport, std::string_view mcap_path) noexcept;
    ~ReplaySource();

    ReplaySource(const ReplaySource&) = delete;
    ReplaySource& operator=(const ReplaySource&) = delete;

    /// Load the MCAP file and register publishers for each recorded channel.
    [[nodiscard]] bool load() noexcept;

    /// Apply a runtime control command.
    void control(const nxd::contracts::ReplayControl& cmd) noexcept;

    /// Emit all recorded samples whose original simulation time is at or before
    /// `sim_time_ns`.  Must be called repeatedly as Chronos advances.
    [[nodiscard]] bool step(std::int64_t sim_time_ns) noexcept;

    /// Current playback state.
    [[nodiscard]] bool is_playing() const noexcept;

    /// Returns true when every message has been emitted and looping is disabled.
    [[nodiscard]] bool finished() const noexcept;

    /// Simulation time of the next sample to emit, or -1 when finished.
    [[nodiscard]] std::int64_t next_sim_time_ns() const noexcept;
  private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace nxd::runtime::recording
