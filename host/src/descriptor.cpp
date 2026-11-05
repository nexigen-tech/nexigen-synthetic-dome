#include "nxd/host/descriptor.hpp"

#define JSON_NOEXCEPTION 1

#include <cctype>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace nxd::host {

    namespace {

        [[nodiscard]] nexigen::core::Result<Version>
        parse_version(const std::string& text) noexcept {
            Version out{};
            std::size_t pos = 0;
            std::uint64_t parts[3] = {};
            for (std::size_t i = 0; i < 3; ++i) {
                if (pos >= text.size()) {
                    return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
                }
                std::uint64_t value = 0;
                std::size_t digits = 0;
                while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
                    value = value * 10 + static_cast<std::uint64_t>(text[pos] - '0');
                    ++pos;
                    ++digits;
                }
                if (digits == 0) {
                    return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
                }
                parts[i] = value;
                if (i < 2) {
                    if (pos >= text.size() || text[pos] != '.') {
                        return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
                    }
                    ++pos;
                }
            }
            if (pos != text.size()) {
                return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
            }
            out.major = parts[0];
            out.minor = parts[1];
            out.patch = parts[2];
            return out;
        }

        [[nodiscard]] bool parse_version_value(const nlohmann::json& j, Version& out) noexcept {
            if (j.is_string()) {
                const auto parsed = parse_version(j.get<std::string>());
                if (!parsed) {
                    return false;
                }
                out = *parsed;
                return true;
            }
            if (j.is_object()) {
                const auto major = j.find("major");
                const auto minor = j.find("minor");
                const auto patch = j.find("patch");
                if (major == j.end() || !major->is_number_unsigned() || minor == j.end() ||
                    !minor->is_number_unsigned() || patch == j.end() ||
                    !patch->is_number_unsigned()) {
                    return false;
                }
                out.major = major->get<std::uint64_t>();
                out.minor = minor->get<std::uint64_t>();
                out.patch = patch->get<std::uint64_t>();
                return true;
            }
            return false;
        }

        [[nodiscard]] bool parse_version_json(const nlohmann::json& j, Version& out) noexcept {
            return parse_version_value(j, out);
        }

        [[nodiscard]] std::string read_text_file(const std::filesystem::path& path) noexcept {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return {};
            }
            std::ostringstream oss;
            oss << file.rdbuf();
            return oss.str();
        }

        [[nodiscard]] bool parse_direction(const std::string& text,
                                           PortDescriptor::Direction& out) noexcept {
            if (text == "in") {
                out = PortDescriptor::Direction::In;
                return true;
            }
            if (text == "out") {
                out = PortDescriptor::Direction::Out;
                return true;
            }
            if (text == "inout") {
                out = PortDescriptor::Direction::InOut;
                return true;
            }
            return false;
        }

        [[nodiscard]] nexigen::core::Result<std::array<std::uint8_t, 32>>
        parse_hex_hash(const std::string& text) noexcept {
            std::array<std::uint8_t, 32> out{};
            std::string trimmed;
            for (char c : text) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    trimmed.push_back(c);
                }
            }
            if (trimmed.size() != 64) {
                return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
            }
            for (std::size_t i = 0; i < 32; ++i) {
                const auto high = static_cast<unsigned char>(trimmed[i * 2]);
                const auto low = static_cast<unsigned char>(trimmed[i * 2 + 1]);
                auto nibble = [](unsigned char c) -> std::optional<std::uint8_t> {
                    if (c >= '0' && c <= '9')
                        return static_cast<std::uint8_t>(c - '0');
                    if (c >= 'a' && c <= 'f')
                        return static_cast<std::uint8_t>(c - 'a' + 10);
                    if (c >= 'A' && c <= 'F')
                        return static_cast<std::uint8_t>(c - 'A' + 10);
                    return std::nullopt;
                };
                const auto h = nibble(high);
                const auto l = nibble(low);
                if (!h || !l) {
                    return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
                }
                out[i] = static_cast<std::uint8_t>((*h << 4) | *l);
            }
            return out;
        }

        [[nodiscard]] bool parse_contracts(const nlohmann::json& j,
                                           std::vector<ContractRef>& out) noexcept {
            if (!j.is_array()) {
                return false;
            }
            for (const auto& item : j) {
                if (!item.is_object()) {
                    return false;
                }
                ContractRef ref;
                const auto name = item.find("name");
                if (name == item.end() || !name->is_string()) {
                    return false;
                }
                ref.name = name->get<std::string>();
                const auto version = item.find("version");
                if (version == item.end() || !parse_version_json(*version, ref.version)) {
                    return false;
                }
                const auto hash = item.find("hash");
                if (hash != item.end()) {
                    if (!hash->is_string()) {
                        return false;
                    }
                    const auto parsed = parse_hex_hash(hash->get<std::string>());
                    if (!parsed) {
                        return false;
                    }
                    ref.hash = *parsed;
                }
                out.push_back(std::move(ref));
            }
            return true;
        }

        [[nodiscard]] bool parse_ports(const nlohmann::json& j,
                                       std::vector<PortDescriptor>& out) noexcept {
            if (!j.is_array()) {
                return false;
            }
            for (const auto& item : j) {
                if (!item.is_object()) {
                    return false;
                }
                PortDescriptor port;
                const auto name = item.find("name");
                if (name == item.end() || !name->is_string()) {
                    return false;
                }
                port.name = name->get<std::string>();
                const auto direction = item.find("direction");
                if (direction == item.end() || !direction->is_string()) {
                    return false;
                }
                if (!parse_direction(direction->get<std::string>(), port.direction)) {
                    return false;
                }
                const auto semantics = item.find("semantics");
                if (semantics == item.end() || !semantics->is_string()) {
                    return false;
                }
                port.semantics = semantics->get<std::string>();
                const auto contract = item.find("contract");
                if (contract == item.end() || !contract->is_string()) {
                    return false;
                }
                port.contract_name = contract->get<std::string>();
                const auto contract_version = item.find("contract_version");
                if (contract_version == item.end() ||
                    !parse_version_json(*contract_version, port.contract_version)) {
                    return false;
                }
                const auto required = item.find("required");
                port.required =
                    (required != item.end() && required->is_boolean() && required->get<bool>());
                out.push_back(std::move(port));
            }
            return true;
        }

        [[nodiscard]] bool parse_parameters(const nlohmann::json& j,
                                            std::vector<ParameterDescriptor>& out) noexcept {
            if (!j.is_array()) {
                return false;
            }
            for (const auto& item : j) {
                if (!item.is_object()) {
                    return false;
                }
                ParameterDescriptor param;
                const auto name = item.find("name");
                if (name == item.end() || !name->is_string()) {
                    return false;
                }
                param.name = name->get<std::string>();
                const auto type = item.find("type");
                if (type == item.end() || !type->is_string()) {
                    return false;
                }
                param.type = type->get<std::string>();
                const auto min = item.find("min");
                if (min != item.end()) {
                    if (!min->is_number_integer() && !min->is_number_unsigned()) {
                        return false;
                    }
                    param.min = min->get<std::int64_t>();
                }
                const auto max = item.find("max");
                if (max != item.end()) {
                    if (!max->is_number_integer() && !max->is_number_unsigned()) {
                        return false;
                    }
                    param.max = max->get<std::int64_t>();
                }
                const auto file = item.find("file");
                param.file = (file != item.end() && file->is_boolean() && file->get<bool>());
                const auto required = item.find("required");
                param.required =
                    (required != item.end() && required->is_boolean() && required->get<bool>());
                out.push_back(std::move(param));
            }
            return true;
        }

        [[nodiscard]] bool parse_thread_roles(const nlohmann::json& j,
                                              std::vector<ThreadRoleDescriptor>& out) noexcept {
            if (!j.is_array()) {
                return false;
            }
            for (const auto& item : j) {
                if (!item.is_object()) {
                    return false;
                }
                ThreadRoleDescriptor role;
                const auto name = item.find("name");
                if (name == item.end() || !name->is_string()) {
                    return false;
                }
                role.name = name->get<std::string>();
                const auto required = item.find("required");
                role.required =
                    (required != item.end() && required->is_boolean() && required->get<bool>());
                const auto single_cpu = item.find("single_cpu");
                role.single_cpu = (single_cpu != item.end() && single_cpu->is_boolean() &&
                                   single_cpu->get<bool>());
                const auto schedule = item.find("default_schedule");
                if (schedule != item.end() && schedule->is_string()) {
                    role.default_schedule = schedule->get<std::string>();
                }
                out.push_back(std::move(role));
            }
            return true;
        }

        [[nodiscard]] bool parse_resources(const nlohmann::json& j,
                                           ResourcesDescriptor& out) noexcept {
            if (!j.is_object()) {
                return false;
            }
            const auto max_heap = j.find("max_heap_bytes");
            if (max_heap != j.end()) {
                if (!max_heap->is_number_integer() && !max_heap->is_number_unsigned()) {
                    return false;
                }
                out.max_heap_bytes = max_heap->get<std::uint64_t>();
            }
            const auto deterministic = j.find("deterministic");
            out.deterministic = (deterministic != j.end() && deterministic->is_boolean() &&
                                 deterministic->get<bool>());
            const auto uses_rng = j.find("uses_rng");
            out.uses_rng = (uses_rng != j.end() && uses_rng->is_boolean() && uses_rng->get<bool>());
            const auto requires_rt = j.find("requires_hardware_rt");
            out.requires_hardware_rt =
                (requires_rt != j.end() && requires_rt->is_boolean() && requires_rt->get<bool>());
            return true;
        }

        [[nodiscard]] bool parse_supported_modes(const nlohmann::json& j,
                                                 std::vector<std::string>& out) noexcept {
            if (!j.is_array()) {
                return false;
            }
            for (const auto& item : j) {
                if (!item.is_string()) {
                    return false;
                }
                out.push_back(item.get<std::string>());
            }
            return true;
        }

    } // namespace

    nexigen::core::Result<ComponentDescriptor>
    load_component_descriptor(const std::filesystem::path& path) noexcept {
        const std::string text = read_text_file(path);
        if (text.empty()) {
            return std::unexpected(nexigen::core::ErrorCode::SystemError);
        }

        nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        ComponentDescriptor desc;
        const auto name = j.find("name");
        if (name == j.end() || !name->is_string()) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }
        desc.name = name->get<std::string>();

        const auto version = j.find("version");
        if (version == j.end() || !parse_version_json(*version, desc.version)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto sdk = j.find("sdk");
        if (sdk == j.end() || !sdk->is_object()) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }
        const auto abi_major = sdk->find("abi_major");
        const auto abi_minor = sdk->find("abi_minor");
        if (abi_major == sdk->end() || !abi_major->is_number_unsigned() ||
            abi_minor == sdk->end() || !abi_minor->is_number_unsigned()) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }
        desc.sdk.abi_major = abi_major->get<std::uint64_t>();
        desc.sdk.abi_minor = abi_minor->get<std::uint64_t>();

        const auto contracts = j.find("contracts");
        if (contracts == j.end() || !parse_contracts(*contracts, desc.contracts)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto ports = j.find("ports");
        if (ports == j.end() || !parse_ports(*ports, desc.ports)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto parameters = j.find("parameters");
        if (parameters == j.end() || !parse_parameters(*parameters, desc.parameters)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto thread_roles = j.find("thread_roles");
        if (thread_roles == j.end() || !parse_thread_roles(*thread_roles, desc.thread_roles)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto resources = j.find("resources");
        if (resources == j.end() || !parse_resources(*resources, desc.resources)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        const auto modes = j.find("supported_modes");
        if (modes == j.end() || !parse_supported_modes(*modes, desc.supported_modes)) {
            return std::unexpected(nexigen::core::ErrorCode::InvalidArgument);
        }

        return desc;
    }

    nexigen::core::Result<std::array<std::uint8_t, 32>>
    load_descriptor_hash(const std::filesystem::path& path) noexcept {
        const std::string text = read_text_file(path);
        if (text.empty()) {
            return std::unexpected(nexigen::core::ErrorCode::SystemError);
        }
        return parse_hex_hash(text);
    }

} // namespace nxd::host
