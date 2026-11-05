#include "nxd/host/headless_driver.hpp"

namespace nxd::host {

    namespace {

        std::uint64_t rng_callback(void* state) noexcept {
            auto* seed = static_cast<std::uint64_t*>(state);
            *seed ^= *seed >> 12;
            *seed ^= *seed << 25;
            *seed ^= *seed >> 27;
            return *seed * 0x2545F4914F6CDD1DULL;
        }

    } // namespace

    HeadlessDriver::HeadlessDriver(ComponentInstance& component) noexcept
        : component_(&component) {}

    DriverResult HeadlessDriver::run(std::uint64_t frame_count, std::uint64_t seed,
                                     std::uint64_t sim_time_step_ns,
                                     std::uint64_t hw_time_step_ns) noexcept {
        DriverResult result{};
        if (!component_->armed()) {
            result.last_status = NXD_ERROR_NOT_INITIALIZED;
            return result;
        }

        std::uint64_t seed_state = seed;
        for (std::uint64_t frame = 0; frame < frame_count; ++frame) {
            NxdFrameContext ctx{};
            ctx.frame_no = frame;
            ctx.sim_time_ns = static_cast<std::int64_t>(frame * sim_time_step_ns);
            ctx.hw_time_ns = static_cast<std::int64_t>(frame * hw_time_step_ns);
            ctx.seed_version = 1;
            ctx.rng = rng_callback;
            ctx.rng_state = &seed_state;

            const auto status = component_->step(&ctx);
            if (!status) {
                result.last_status = component_->last_status();
                result.failed_frame = frame;
                return result;
            }
            ++result.frames_completed;
        }

        component_->stop();
        result.stopped_normally = true;
        return result;
    }

} // namespace nxd::host
