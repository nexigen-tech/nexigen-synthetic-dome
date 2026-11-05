/* Host plugin loading and pre-initialization validation tests.
 *
 * Verifies that every ABI, identity, descriptor-hash, and resource mismatch is
 * rejected before the plugin's initialize entry point is invoked.
 */

#include "nxd/host/descriptor.hpp"
#include "nxd/host/plugin.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

    bool load_succeeded(const nxd::host::LoadedPlugin& plugin) {
        return plugin.handle != nullptr && plugin.table != nullptr;
    }

    bool valid_load(const char* descriptor_path, const char* hash_path, const char* plugin_path) {
        const auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
        if (!descriptor) {
            std::cerr << "failed to load descriptor: " << descriptor_path << "\n";
            return false;
        }

        const auto hash = nxd::host::load_descriptor_hash(hash_path);
        if (!hash) {
            std::cerr << "failed to load hash: " << hash_path << "\n";
            return false;
        }

        auto plugin = nxd::host::load_plugin(plugin_path, *descriptor, *hash);
        if (!plugin) {
            std::cerr << "valid load failed unexpectedly\n";
            return false;
        }
        if (!load_succeeded(*plugin)) {
            std::cerr << "valid plugin has no handle/table\n";
            return false;
        }
        nxd::host::unload_plugin(*plugin);
        return true;
    }

    bool invalid_load_is_rejected(const char* descriptor_path, const char* hash_path,
                                  const char* plugin_path, const char* label) {
        const auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
        if (!descriptor) {
            std::cerr << label << ": failed to load descriptor\n";
            return false;
        }
        const auto hash = nxd::host::load_descriptor_hash(hash_path);
        if (!hash) {
            std::cerr << label << ": failed to load hash\n";
            return false;
        }
        auto plugin = nxd::host::load_plugin(plugin_path, *descriptor, *hash);
        if (plugin) {
            std::cerr << label << ": load should have been rejected\n";
            nxd::host::unload_plugin(*plugin);
            return false;
        }
        return true;
    }

    bool identity_mismatch_is_rejected(const char* descriptor_path, const char* hash_path,
                                       const char* plugin_path) {
        auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
        if (!descriptor) {
            std::cerr << "identity mismatch: failed to load descriptor\n";
            return false;
        }
        descriptor->name = "WrongName";
        const auto hash = nxd::host::load_descriptor_hash(hash_path);
        if (!hash) {
            std::cerr << "identity mismatch: failed to load hash\n";
            return false;
        }
        auto plugin = nxd::host::load_plugin(plugin_path, *descriptor, *hash);
        if (plugin) {
            std::cerr << "identity mismatch: load should have been rejected\n";
            nxd::host::unload_plugin(*plugin);
            return false;
        }
        return true;
    }

    bool hardware_rt_rejected(const char* descriptor_path, const char* hash_path,
                              const char* plugin_path) {
        auto descriptor = nxd::host::load_component_descriptor(descriptor_path);
        if (!descriptor) {
            std::cerr << "hardware rt: failed to load descriptor\n";
            return false;
        }
        descriptor->resources.requires_hardware_rt = true;
        const auto hash = nxd::host::load_descriptor_hash(hash_path);
        if (!hash) {
            std::cerr << "hardware rt: failed to load hash\n";
            return false;
        }
        nxd::host::HostCapabilities caps{};
        caps.hardware_rt = false;
        auto plugin = nxd::host::load_plugin(plugin_path, *descriptor, *hash, caps);
        if (plugin) {
            std::cerr << "hardware rt: load should have been rejected\n";
            nxd::host::unload_plugin(*plugin);
            return false;
        }
        return true;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 9) {
        std::cerr << "usage: " << argv[0]
                  << " <descriptor.json> <descriptor.hash> <valid-plugin.so>"
                     " <bad-abi-version.so> <bad-table-size.so> <bad-fingerprint.so>"
                     " <bad-descriptor-hash.so> <bad-missing-fn.so>\n";
        return 1;
    }

    const char* descriptor_path = argv[1];
    const char* hash_path = argv[2];
    const char* valid_plugin = argv[3];
    const char* bad_abi_version = argv[4];
    const char* bad_table_size = argv[5];
    const char* bad_fingerprint = argv[6];
    const char* bad_descriptor_hash = argv[7];
    const char* bad_missing_fn = argv[8];

    if (!valid_load(descriptor_path, hash_path, valid_plugin)) {
        return 2;
    }
    if (!invalid_load_is_rejected(descriptor_path, hash_path, bad_abi_version, "abi version")) {
        return 3;
    }
    if (!invalid_load_is_rejected(descriptor_path, hash_path, bad_table_size, "table size")) {
        return 4;
    }
    if (!invalid_load_is_rejected(descriptor_path, hash_path, bad_fingerprint, "fingerprint")) {
        return 5;
    }
    if (!invalid_load_is_rejected(descriptor_path, hash_path, bad_descriptor_hash,
                                  "descriptor hash")) {
        return 6;
    }
    if (!invalid_load_is_rejected(descriptor_path, hash_path, bad_missing_fn, "missing fn")) {
        return 7;
    }
    if (!identity_mismatch_is_rejected(descriptor_path, hash_path, valid_plugin)) {
        return 8;
    }
    if (!hardware_rt_rejected(descriptor_path, hash_path, valid_plugin)) {
        return 9;
    }

    std::cout << "ok\n";
    return 0;
}
