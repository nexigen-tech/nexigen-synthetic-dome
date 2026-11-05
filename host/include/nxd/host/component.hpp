#pragma once

#include <cstddef>
#include <cstdint>

#include "nxd/abi.hpp"
#include "nxd/host/plugin.hpp"
#include <nexigen/core/result.hpp>

namespace nxd::host {

    /// RAII wrapper around a plugin component lifecycle.
    ///
    /// The wrapper calls `create`, `initialize`, `arm`, and `step` in order.  Any
    /// failure in `initialize` or `arm` runs `destroy`.  A `step` failure runs
    /// `stop` then `destroy`.  The destructor cleans up any remaining state.
    class ComponentInstance {
      public:
        explicit ComponentInstance(const LoadedPlugin& plugin) noexcept;
        ~ComponentInstance() noexcept;

        ComponentInstance(const ComponentInstance&) = delete;
        ComponentInstance& operator=(const ComponentInstance&) = delete;
        ComponentInstance(ComponentInstance&&) = delete;
        ComponentInstance& operator=(ComponentInstance&&) = delete;

        [[nodiscard]] nexigen::core::Status create() noexcept;
        [[nodiscard]] nexigen::core::Status initialize(const char* config,
                                                       std::size_t config_len) noexcept;
        [[nodiscard]] nexigen::core::Status arm(const NxdFrameContext* ctx) noexcept;
        [[nodiscard]] nexigen::core::Status step(const NxdFrameContext* ctx) noexcept;

        /// Call the plugin `stop` lifecycle function.  Safe to call multiple times.
        void stop() noexcept;

        /// Stop and destroy the component.  Idempotent and safe after any stage.
        void shutdown() noexcept;

        [[nodiscard]] NxdStatus last_status() const noexcept {
            return last_status_;
        }
        [[nodiscard]] bool created() const noexcept {
            return state_ != nullptr;
        }
        [[nodiscard]] bool initialized() const noexcept {
            return initialized_;
        }
        [[nodiscard]] bool armed() const noexcept {
            return armed_;
        }

      private:
        void do_stop() noexcept;
        void destroy() noexcept;

        const LoadedPlugin* plugin_;
        NxdComponentState* state_ = nullptr;
        bool initialized_ = false;
        bool armed_ = false;
        bool stopped_ = false;
        NxdStatus last_status_ = NXD_OK;
    };

} // namespace nxd::host
