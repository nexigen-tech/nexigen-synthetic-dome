#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nexigen/core/result.hpp>

namespace nxd::host {

    struct Version {
        std::uint64_t major = 0;
        std::uint64_t minor = 0;
        std::uint64_t patch = 0;
    };

    [[nodiscard]] constexpr bool operator==(const Version& a, const Version& b) noexcept {
        return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
    }

    struct ContractRef {
        std::string name;
        Version version;
        std::optional<std::array<std::uint8_t, 32>> hash;
    };

    struct PortDescriptor {
        enum class Direction : std::uint8_t { In, Out, InOut };

        std::string name;
        Direction direction = Direction::In;
        std::string semantics;
        std::string contract_name;
        Version contract_version;
        std::optional<std::array<std::uint8_t, 32>> layout_hash;
        bool required = false;
    };

    struct ParameterDescriptor {
        std::string name;
        std::string type;
        std::optional<std::int64_t> min;
        std::optional<std::int64_t> max;
        bool file = false;
        bool required = false;
    };

    struct ThreadRoleDescriptor {
        std::string name;
        bool required = false;
        bool single_cpu = false;
        std::optional<std::string> default_schedule;
    };

    struct ResourcesDescriptor {
        std::optional<std::uint64_t> max_heap_bytes;
        bool deterministic = false;
        bool uses_rng = false;
        bool requires_hardware_rt = false;
    };

    struct ComponentDescriptor {
        std::string name;
        Version version;
        struct SdkVersion {
            std::uint64_t abi_major = 0;
            std::uint64_t abi_minor = 0;
        } sdk;
        std::vector<ContractRef> contracts;
        std::vector<PortDescriptor> ports;
        std::vector<ParameterDescriptor> parameters;
        std::vector<ThreadRoleDescriptor> thread_roles;
        ResourcesDescriptor resources;
        std::vector<std::string> supported_modes;
        std::array<std::uint8_t, 32> descriptor_hash{};
    };

    [[nodiscard]] nexigen::core::Result<ComponentDescriptor>
    load_component_descriptor(const std::filesystem::path& path) noexcept;

    [[nodiscard]] nexigen::core::Result<std::array<std::uint8_t, 32>>
    load_descriptor_hash(const std::filesystem::path& path) noexcept;

} // namespace nxd::host
