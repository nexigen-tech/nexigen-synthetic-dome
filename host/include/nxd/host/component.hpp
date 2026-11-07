#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nxd/abi.hpp"
#include "nxd/host/descriptor.hpp"
#include "nxd/host/plugin.hpp"
#include <nexigen/core/result.hpp>

namespace nxd::host {

/// Binding for one typed port on a component instance.
struct PortBinding {
    std::string name;
    PortDescriptor::Direction direction = PortDescriptor::Direction::In;
    std::size_t element_size = 0;
    std::size_t count = 0;
    std::optional<std::array<std::uint8_t, 32>> layout_hash;
    std::vector<std::byte> buffer;
};

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

    /// Configures typed port bindings.  Must be called after `create` and
    /// before `arm`.  The host validates bindings against the descriptor
    /// and the plugin's extended callback during `arm`.
    void configure_ports(std::vector<PortBinding> bindings) noexcept;

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
    [[nodiscard]] const std::vector<PortBinding>& port_bindings() const noexcept {
        return bindings_;
    }
    [[nodiscard]] std::vector<PortBinding>& port_bindings() noexcept {
        return bindings_;
    }
  private:
    void do_stop() noexcept;
    void destroy() noexcept;

    [[nodiscard]] bool validate_port_bindings() noexcept;
    [[nodiscard]] NxdStatus invoke_step_io(const NxdFrameContext* ctx) noexcept;

    const LoadedPlugin* plugin_;
    NxdComponentState* state_ = nullptr;
    std::vector<PortBinding> bindings_;
    std::vector<NxdPortSlot> input_slots_;
    std::vector<NxdPortSlot> output_slots_;
    bool initialized_ = false;
    bool armed_ = false;
    bool stopped_ = false;
    NxdStatus last_status_ = NXD_OK;
};

} // namespace nxd::host
