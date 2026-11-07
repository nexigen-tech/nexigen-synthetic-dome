#include "nxd/host/plugin.hpp"

#include <cstring>
#include <dlfcn.h>

namespace nxd::host {

namespace {

[[nodiscard]] bool fingerprint_equal(const NxdAbiFingerprint& a,
                                     const NxdAbiFingerprint& b) noexcept {
    return std::memcmp(a.value, b.value, sizeof(a.value)) == 0;
}

[[nodiscard]] bool hash_equal(const std::array<std::uint8_t, 32>& a,
                              const std::array<std::uint8_t, 32>& b) noexcept {
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

[[nodiscard]] bool name_equal(const char (&table_name)[64],
                              const std::string& descriptor_name) noexcept {
    const std::size_t len = std::strlen(table_name);
    if (len != descriptor_name.size()) {
        return false;
    }
    return std::memcmp(table_name, descriptor_name.data(), len) == 0;
}

[[nodiscard]] bool version_equal(const NxdAbiVersion& table_version,
                                 const Version& desc_version) noexcept {
    return table_version.major == desc_version.major &&
           table_version.minor == desc_version.minor &&
           table_version.patch == desc_version.patch;
}

[[nodiscard]] NxdAbiFingerprint abi_1_target() noexcept {
    NxdAbiFingerprint out{};
    static_assert(sizeof(NXD_ABI_1_TARGET) == sizeof(out.value));
    std::memcpy(out.value, NXD_ABI_1_TARGET, sizeof(out.value));
    return out;
}

[[nodiscard]] std::array<std::uint8_t, 32> table_descriptor_hash(
    const NxdAbiTable* t) noexcept {
    std::array<std::uint8_t, 32> h{};
    std::memcpy(h.data(), t->descriptor_hash.value, sizeof(h));
    return h;
}

void close_and_reset(LoadedPlugin& plugin) noexcept {
    if (plugin.handle != nullptr) {
        ::dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    plugin.table = nullptr;
}

[[nodiscard]] bool open_library(const std::filesystem::path& plugin_path,
                                LoadedPlugin& plugin) noexcept {
    plugin.handle = ::dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    return plugin.handle != nullptr;
}

[[nodiscard]] const NxdAbiTable* load_abi_table(void* handle) noexcept {
    const auto entry = reinterpret_cast<NxdAbiEntryFn>(::dlsym(handle, "nxd_abi_entry"));
    if (entry == nullptr) {
        return nullptr;
    }
    return entry();
}

[[nodiscard]] bool verify_abi_version(const NxdAbiTable* t,
                                      const ComponentDescriptor& descriptor) noexcept {
    return t->abi_version.major == descriptor.sdk.abi_major &&
           t->abi_version.minor >= descriptor.sdk.abi_minor && t->abi_version.major != 0;
}

[[nodiscard]] bool verify_table_integrity(const NxdAbiTable* t) noexcept {
    if (t->table_size != sizeof(NxdAbiTable)) {
        return false;
    }
    return fingerprint_equal(t->fingerprint, abi_1_target());
}

[[nodiscard]] bool verify_component_identity(const NxdAbiTable* t,
                                             const ComponentDescriptor& descriptor) noexcept {
    return name_equal(t->component_name, descriptor.name) &&
           version_equal(t->component_version, descriptor.version);
}

[[nodiscard]] bool verify_descriptor_hash(const NxdAbiTable* t,
                                          const std::array<std::uint8_t, 32>& expected_hash) noexcept {
    return hash_equal(expected_hash, table_descriptor_hash(t));
}

[[nodiscard]] bool verify_required_functions(const NxdAbiTable* t) noexcept {
    return t->create != nullptr && t->destroy != nullptr && t->initialize != nullptr &&
           t->arm != nullptr && t->step != nullptr && t->stop != nullptr;
}

[[nodiscard]] bool check_hardware_rt(const ComponentDescriptor& descriptor,
                                     const HostCapabilities& caps) noexcept {
    return !(descriptor.resources.requires_hardware_rt && !caps.hardware_rt);
}

} // namespace

nexigen::core::ErrorCode nxd_status_to_error(NxdStatus status) noexcept {
    switch (status) {
    case NXD_OK:
        return nexigen::core::ErrorCode::Ok;
    case NXD_ERROR_INVALID_ARGUMENT:
        return nexigen::core::ErrorCode::InvalidArgument;
    case NXD_ERROR_NOT_INITIALIZED:
        return nexigen::core::ErrorCode::NotInitialized;
    case NXD_ERROR_ALREADY_INITIALIZED:
        return nexigen::core::ErrorCode::Conflict;
    default:
        return nexigen::core::ErrorCode::SystemError;
    }
}

nexigen::core::Result<LoadedPlugin>
load_plugin(const std::filesystem::path& plugin_path, const ComponentDescriptor& descriptor,
            const std::array<std::uint8_t, 32>& expected_hash,
            const HostCapabilities& caps) noexcept {
    LoadedPlugin plugin;
    plugin.path = plugin_path;
    plugin.descriptor = descriptor;
    plugin.expected_descriptor_hash = expected_hash;

    if (!open_library(plugin_path, plugin)) {
        return std::unexpected(nexigen::core::ErrorCode::SystemError);
    }

    plugin.table = load_abi_table(plugin.handle);
    if (plugin.table == nullptr) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    const auto* t = plugin.table;
    if (!verify_abi_version(t, descriptor)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    if (!verify_table_integrity(t)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    if (!verify_component_identity(t, descriptor)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    if (!verify_descriptor_hash(t, expected_hash)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    if (!verify_required_functions(t)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    // step_io is optional; null means the plugin uses the legacy step callback.

    if (!check_hardware_rt(descriptor, caps)) {
        close_and_reset(plugin);
        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
    }

    return plugin;
}

void unload_plugin(LoadedPlugin& plugin) noexcept {
    if (plugin.handle != nullptr) {
        ::dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    plugin.table = nullptr;
}

} // namespace nxd::host
