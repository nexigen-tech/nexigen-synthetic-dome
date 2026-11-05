#include "nxd/runtime/chronos/state_machine.hpp"

#include <expected>
#include <utility>

namespace nxd::runtime::chronos {

Runtime::Runtime(Config cfg) noexcept : cfg_(cfg) {}

nexigen::core::Result<Partition*> Runtime::register_partition(std::uint32_t id,
                                                               std::string name) noexcept {
    if (state_.load(std::memory_order_acquire) != State::Initialized) {
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    auto partition = std::make_unique<Partition>(id, std::move(name));
    Partition* const raw = partition.get();
    partitions_.emplace_back(std::move(partition));
    return raw;
}

nexigen::core::Status Runtime::initialize() noexcept {
    if (!is_valid(cfg_)) {
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    const auto status = try_transition(State::Uninitialized, State::Initialized);
    if (!status) {
        return status;
    }

    frame_no_.store(0, std::memory_order_relaxed);
    sim_time_ = SimTime{0};
    wall_time_ = WallTime{0};
    hw_fpga_time_ = HwFpgaTime{0};
    return {};
}

nexigen::core::Status Runtime::arm() noexcept {
    const auto status = try_transition(State::Initialized, State::Armed);
    if (!status) {
        return status;
    }

    frame_no_.store(0, std::memory_order_relaxed);
    sim_time_ = SimTime{0};
    wall_time_ = WallTime{0};
    hw_fpga_time_ = HwFpgaTime{0};
    return {};
}

nexigen::core::Status Runtime::play() noexcept {
    if (cfg_.mode == Mode::Step) {
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    State current = state_.load(std::memory_order_acquire);
    if (current != State::Armed && current != State::Paused) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }

    State expected = current;
    if (!state_.compare_exchange_strong(expected, State::Playing,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }
    return {};
}

nexigen::core::Status Runtime::pause() noexcept {
    const auto status = try_transition(State::Playing, State::Paused);
    if (!status) {
        return status;
    }
    return {};
}

nexigen::core::Status Runtime::step() noexcept {
    State expected = State::Armed;
    if (!state_.compare_exchange_strong(expected, State::Stepping,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }

    const std::uint64_t next_frame = frame_no_.load(std::memory_order_relaxed) + 1;
    frame_no_.store(next_frame, std::memory_order_relaxed);

    sim_time_ = sim_time_ + duration_ns(cfg_.base_frame_interval_ns);

    switch (cfg_.mode) {
        case Mode::RealTimeHil:
            wall_time_ = wall_time_ + duration_ns(cfg_.base_frame_interval_ns);
            hw_fpga_time_ = hw_fpga_time_ + duration_ns(cfg_.base_frame_interval_ns);
            break;
        case Mode::ScaledSil: {
            const std::int64_t wall_step = cfg_.base_frame_interval_ns / cfg_.scale_factor;
            wall_time_ = wall_time_ + duration_ns(wall_step);
            break;
        }
        case Mode::FreeSil:
            // Free SIL advances logical time as fast as possible; wall time is
            // not a pacing target in this test path.
            break;
        case Mode::Step:
            wall_time_ = wall_time_ + duration_ns(cfg_.base_frame_interval_ns);
            break;
    }

    publish_current_frame_context();

    state_.store(State::Armed, std::memory_order_release);
    return {};
}

void Runtime::publish_current_frame_context() noexcept {
    const FrameContext ctx{
        .frame_no = frame_no_.load(std::memory_order_relaxed),
        .sim_time = sim_time_,
        .wall_time = wall_time_,
        .hw_fpga_time = hw_fpga_time_,
        .has_hw_fpga_time = cfg_.mode == Mode::RealTimeHil,
    };

    for (const auto& partition : partitions_) {
        if (partition != nullptr) {
            partition->publish(ctx);
        }
    }
}

nexigen::core::Status Runtime::stop() noexcept {
    State current = state_.load(std::memory_order_acquire);
    if (current != State::Initialized && current != State::Armed &&
        current != State::Playing && current != State::Paused) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }

    State expected = current;
    if (!state_.compare_exchange_strong(expected, State::Stopped,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }
    return {};
}

nexigen::core::Status Runtime::dispose() noexcept {
    return try_transition(State::Stopped, State::Disposed);
}

State Runtime::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

Mode Runtime::mode() const noexcept {
    return cfg_.mode;
}

std::uint64_t Runtime::frame_no() const noexcept {
    return frame_no_.load(std::memory_order_acquire);
}

SimTime Runtime::sim_time() const noexcept {
    return sim_time_;
}

WallTime Runtime::wall_time() const noexcept {
    return wall_time_;
}

HwFpgaTime Runtime::hw_fpga_time() const noexcept {
    return hw_fpga_time_;
}

nexigen::core::Status
Runtime::try_transition(State expected, State next) noexcept {
    State current = state_.load(std::memory_order_acquire);
    if (current != expected) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }

    if (!state_.compare_exchange_strong(current, next,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }
    return {};
}

bool Runtime::is_valid(const Config& cfg) noexcept {
    if (cfg.base_frame_interval_ns <= 0) {
        return false;
    }
    if (cfg.scale_factor == 0) {
        return false;
    }
    if (cfg.mode == Mode::ScaledSil &&
        cfg.base_frame_interval_ns % cfg.scale_factor != 0) {
        return false;
    }
    return true;
}

nexigen::core::ErrorCode to_error_code(Error err) noexcept {
    switch (err) {
        case Error::None:
            return nexigen::core::ErrorCode::Ok;
        case Error::InvalidState:
        case Error::InvalidMode:
        case Error::InvalidConfiguration:
            return nexigen::core::ErrorCode::InvalidArgument;
    }
    return nexigen::core::ErrorCode::InvalidArgument;
}

} // namespace nxd::runtime::chronos
