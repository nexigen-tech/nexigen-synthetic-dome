#include "nxd/host/component.hpp"

namespace nxd::host {

    ComponentInstance::ComponentInstance(const LoadedPlugin& plugin) noexcept : plugin_(&plugin) {}

    ComponentInstance::~ComponentInstance() noexcept {
        shutdown();
    }

    nexigen::core::Status ComponentInstance::create() noexcept {
        if (state_ != nullptr) {
            return std::unexpected(nexigen::core::ErrorCode::Conflict);
        }
        state_ = plugin_->table->create();
        if (state_ == nullptr) {
            last_status_ = NXD_ERROR_GENERAL;
            return std::unexpected(nexigen::core::ErrorCode::SystemError);
        }
        return {};
    }

    nexigen::core::Status ComponentInstance::initialize(const char* config,
                                                        std::size_t config_len) noexcept {
        if (state_ == nullptr) {
            return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
        }
        if (initialized_) {
            return std::unexpected(nexigen::core::ErrorCode::Conflict);
        }
        const NxdStatus status = plugin_->table->initialize(state_, config, config_len);
        if (status != NXD_OK) {
            last_status_ = status;
            destroy();
            return std::unexpected(nxd_status_to_error(status));
        }
        initialized_ = true;
        return {};
    }

    nexigen::core::Status ComponentInstance::arm(const NxdFrameContext* ctx) noexcept {
        if (state_ == nullptr) {
            return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
        }
        if (armed_) {
            return std::unexpected(nexigen::core::ErrorCode::Conflict);
        }
        const NxdStatus status = plugin_->table->arm(state_, ctx);
        if (status != NXD_OK) {
            last_status_ = status;
            destroy();
            return std::unexpected(nxd_status_to_error(status));
        }
        armed_ = true;
        return {};
    }

    nexigen::core::Status ComponentInstance::step(const NxdFrameContext* ctx) noexcept {
        if (state_ == nullptr || !armed_) {
            return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
        }
        const NxdStatus status = plugin_->table->step(state_, ctx);
        if (status != NXD_OK) {
            last_status_ = status;
            stop();
            destroy();
            return std::unexpected(nxd_status_to_error(status));
        }
        return {};
    }

    void ComponentInstance::stop() noexcept {
        do_stop();
    }

    void ComponentInstance::do_stop() noexcept {
        if (state_ != nullptr && !stopped_ && (initialized_ || armed_)) {
            plugin_->table->stop(state_);
            stopped_ = true;
        }
    }

    void ComponentInstance::destroy() noexcept {
        do_stop();
        if (state_ != nullptr) {
            plugin_->table->destroy(state_);
            state_ = nullptr;
        }
        initialized_ = false;
        armed_ = false;
    }

    void ComponentInstance::shutdown() noexcept {
        destroy();
    }

} // namespace nxd::host
