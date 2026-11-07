#include "nxd/host/component.hpp"

#include <cstring>
#include <format>

namespace nxd::host {

namespace {

[[nodiscard]] std::uint64_t fnv1a_hash(std::string_view name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

[[nodiscard]] bool direction_is_input(PortDescriptor::Direction dir) noexcept {
    return dir == PortDescriptor::Direction::In ||
           dir == PortDescriptor::Direction::InOut;
}

[[nodiscard]] bool direction_is_output(PortDescriptor::Direction dir) noexcept {
    return dir == PortDescriptor::Direction::Out ||
           dir == PortDescriptor::Direction::InOut;
}

[[nodiscard]] bool hash_equal(const std::array<std::uint8_t, 32>& a,
                              const std::array<std::uint8_t, 32>& b) noexcept {
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

} // namespace

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

void ComponentInstance::configure_ports(std::vector<PortBinding> bindings) noexcept {
    bindings_ = std::move(bindings);
    for (auto& binding : bindings_) {
        binding.buffer.resize(binding.element_size * binding.count);
    }
}

bool ComponentInstance::validate_port_bindings() noexcept {
    const auto& descriptor = plugin_->descriptor;

    // Legacy plugins that do not export step_io are not required to provide
    // typed port bindings; the host calls their legacy step callback and
    // leaves descriptor ports unvalidated.
    if (plugin_->table->step_io == nullptr) {
        return true;
    }

    for (const auto& desc_port : descriptor.ports) {
        const auto it = std::find_if(
            bindings_.begin(), bindings_.end(),
            [&desc_port](const PortBinding& b) { return b.name == desc_port.name; });

        if (desc_port.required && it == bindings_.end()) {
            last_status_ = NXD_ERROR_ABI_MISMATCH;
            return false;
        }
        if (it == bindings_.end()) {
            continue;
        }

        const bool needs_input = direction_is_input(desc_port.direction);
        const bool needs_output = direction_is_output(desc_port.direction);
        const bool provides_input = direction_is_input(it->direction);
        const bool provides_output = direction_is_output(it->direction);
        if ((needs_input && !provides_input) || (needs_output && !provides_output)) {
            last_status_ = NXD_ERROR_ABI_MISMATCH;
            return false;
        }

        if (it->element_size == 0 || it->count == 0) {
            last_status_ = NXD_ERROR_ABI_MISMATCH;
            return false;
        }

        if (desc_port.layout_hash.has_value() && it->layout_hash.has_value()) {
            if (!hash_equal(*desc_port.layout_hash, *it->layout_hash)) {
                last_status_ = NXD_ERROR_ABI_MISMATCH;
                return false;
            }
        }
    }

    for (const auto& binding : bindings_) {
        const auto it = std::find_if(
            descriptor.ports.begin(), descriptor.ports.end(),
            [&binding](const PortDescriptor& p) { return p.name == binding.name; });
        if (it == descriptor.ports.end()) {
            last_status_ = NXD_ERROR_ABI_MISMATCH;
            return false;
        }
    }

    if (!bindings_.empty() && plugin_->table->step_io == nullptr) {
        last_status_ = NXD_ERROR_ABI_MISMATCH;
        return false;
    }

    return true;
}

nexigen::core::Status ComponentInstance::arm(const NxdFrameContext* ctx) noexcept {
    if (state_ == nullptr) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }
    if (armed_) {
        return std::unexpected(nexigen::core::ErrorCode::Conflict);
    }
    if (!validate_port_bindings()) {
        destroy();
        return std::unexpected(nxd_status_to_error(last_status_));
    }

    input_slots_.clear();
    output_slots_.clear();
    for (auto& binding : bindings_) {
        NxdPortSlot slot{};
        slot.port_name_hash = fnv1a_hash(binding.name);
        slot.data = static_cast<void*>(binding.buffer.data());
        slot.count = binding.count;
        slot.element_size = binding.element_size;
        if (binding.layout_hash.has_value()) {
            std::memcpy(slot.layout_hash.value, binding.layout_hash->data(),
                        sizeof(slot.layout_hash.value));
        }
        if (direction_is_input(binding.direction)) {
            input_slots_.push_back(slot);
        }
        if (direction_is_output(binding.direction)) {
            output_slots_.push_back(slot);
        }
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

NxdStatus ComponentInstance::invoke_step_io(const NxdFrameContext* ctx) noexcept {
    NxdPortIoBundle inputs{};
    inputs.count = input_slots_.size();
    inputs.slots = input_slots_.data();

    NxdPortIoBundle outputs{};
    outputs.count = output_slots_.size();
    outputs.slots = output_slots_.data();

    return plugin_->table->step_io(state_, ctx, &inputs, &outputs);
}

nexigen::core::Status ComponentInstance::step(const NxdFrameContext* ctx) noexcept {
    if (state_ == nullptr || !armed_) {
        return std::unexpected(nexigen::core::ErrorCode::NotInitialized);
    }

    NxdStatus status = NXD_OK;
    if (!bindings_.empty() && plugin_->table->step_io != nullptr) {
        status = invoke_step_io(ctx);
    } else {
        status = plugin_->table->step(state_, ctx);
    }

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
