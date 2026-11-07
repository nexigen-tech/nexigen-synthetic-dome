#include "nxd/host/harness.hpp"

#include "nxd/host/thread_roles.hpp"

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

HeadlessHarness::HeadlessHarness(const std::filesystem::path& plugin_path,
                                 const std::filesystem::path& descriptor_path,
                                 const HostCapabilities& caps) noexcept
    : plugin_path_(plugin_path), descriptor_path_(descriptor_path), caps_(caps) {}

nexigen::core::Status HeadlessHarness::load() noexcept {
    const auto descriptor = load_component_descriptor(descriptor_path_);
    if (!descriptor) {
        return std::unexpected(descriptor.error());
    }

    const auto hash_path = std::filesystem::path(descriptor_path_).replace_extension("hash");
    const auto hash = load_descriptor_hash(hash_path);
    if (!hash) {
        return std::unexpected(hash.error());
    }

    const auto plugin = load_plugin(plugin_path_, *descriptor, *hash, caps_);
    if (!plugin) {
        return std::unexpected(plugin.error());
    }

    plugin_ = *plugin;
    component_.emplace(*plugin_);
    return {};
}

nexigen::core::Status HeadlessHarness::initialize(std::string_view config) noexcept {
    if (!component_) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }
    if (const auto status = component_->create(); !status) {
        return status;
    }
    return component_->initialize(config.data(), config.size());
}

void HeadlessHarness::configure_ports(std::vector<PortBinding> bindings) noexcept {
    bindings_ = std::move(bindings);
    if (component_) {
        component_->configure_ports(bindings_);
    }
}

nexigen::core::Status HeadlessHarness::arm(std::uint64_t seed) noexcept {
    if (!component_) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }

    if (!plugin_) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }

    auto outcome = apply_thread_roles(plugin_->descriptor, /*best_effort_only=*/true);
    thread_reports_ = std::move(outcome.reports);
    if (!outcome.status) {
        return outcome.status;
    }

    seed_ = seed;
    std::uint64_t seed_state = seed;
    NxdFrameContext ctx{};
    ctx.frame_no = 0;
    ctx.sim_time_ns = 0;
    ctx.hw_time_ns = 0;
    ctx.seed_version = 1;
    ctx.rng = rng_callback;
    ctx.rng_state = &seed_state;
    return component_->arm(&ctx);
}

nexigen::core::Status HeadlessHarness::run(std::uint64_t frame_count,
                                           std::uint64_t sim_time_step_ns,
                                           std::uint64_t hw_time_step_ns) noexcept {
    if (!component_) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }
    HeadlessDriver driver(*component_);
    last_result_ =
        driver.run(frame_count, seed_, sim_time_step_ns, hw_time_step_ns, frames_run_);
    if (!last_result_.stopped_normally) {
        return std::unexpected(nexigen::core::ErrorCode::SystemError);
    }
    frames_run_ += frame_count;
    return {};
}

std::uint64_t HeadlessHarness::frames_run() const noexcept {
    return frames_run_;
}

const LoadedPlugin* HeadlessHarness::plugin() const noexcept {
    return plugin_ ? &*plugin_ : nullptr;
}

ComponentInstance* HeadlessHarness::component() noexcept {
    return component_ ? &*component_ : nullptr;
}

const DriverResult& HeadlessHarness::last_result() const noexcept {
    return last_result_;
}

const std::vector<ThreadRoleReport>& HeadlessHarness::thread_reports() const noexcept {
    return thread_reports_;
}

std::uint64_t HeadlessHarness::seed() const noexcept {
    return seed_;
}

} // namespace nxd::host
