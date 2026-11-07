#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "nxd/abi.hpp"
#include "nxd/host/descriptor.hpp"
#include <nexigen/core/result.hpp>

namespace nxd::host {

/// Capabilities of the host runtime.  A mismatch between a plugin's declared
/// requirements and the host's capabilities aborts loading before any lifecycle
/// function is invoked.
struct HostCapabilities {
    bool hardware_rt = false;
};

/// A successfully loaded and validated plugin.  The shared library handle is
/// owned by this value and released by `unload_plugin` or by destroying the
/// value after moving it back out.
struct LoadedPlugin {
    std::filesystem::path path;
    void* handle = nullptr;
    const NxdAbiTable* table = nullptr;
    ComponentDescriptor descriptor;
    std::array<std::uint8_t, 32> expected_descriptor_hash{};
};

/// Load a shared plugin library and validate it against the supplied descriptor
/// and expected descriptor hash.  Returns an error before `initialize` would be
/// called for any mismatch in ABI, fingerprint, identity, hash, or resources.
[[nodiscard]] nexigen::core::Result<LoadedPlugin>
load_plugin(const std::filesystem::path& plugin_path, const ComponentDescriptor& descriptor,
            const std::array<std::uint8_t, 32>& expected_hash,
            const HostCapabilities& caps = {}) noexcept;

/// Release the shared library handle.  Safe to call on a default-constructed
/// value or after a failed partial load.
void unload_plugin(LoadedPlugin& plugin) noexcept;

/// Convert a numeric ABI status into the core error vocabulary.
[[nodiscard]] nexigen::core::ErrorCode nxd_status_to_error(NxdStatus status) noexcept;

} // namespace nxd::host
