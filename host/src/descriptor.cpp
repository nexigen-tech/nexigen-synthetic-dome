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

[[nodiscard]] bool parse_required_string(const nlohmann::json& obj,
                                         std::string_view key,
                                         std::string& out) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) {
        return false;
    }
    out = it->get<std::string>();
    return true;
}

[[nodiscard]] bool parse_required_object(const nlohmann::json& obj,
                                         std::string_view key,
                                         const nlohmann::json*& out) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_object()) {
        return false;
    }
    out = &*it;
    return true;
}

template <typename T, typename ParseFn>
[[nodiscard]] bool parse_array(const nlohmann::json& j,
                               std::vector<T>& out,
                               const ParseFn& parse) noexcept {
    if (!j.is_array()) {
        return false;
    }
    for (const auto& item : j) {
        const auto parsed = parse(item);
        if (!parsed.has_value()) {
            return false;
        }
        out.push_back(*parsed);
    }
    return true;
}

[[nodiscard]] std::optional<ContractRef> contract_from_json(
    const nlohmann::json& item) noexcept {
    if (!item.is_object()) {
        return std::nullopt;
    }
    ContractRef ref;
    if (!parse_required_string(item, "name", ref.name)) {
        return std::nullopt;
    }
    const auto version = item.find("version");
    if (version == item.end() || !parse_version_json(*version, ref.version)) {
        return std::nullopt;
    }
    const auto hash = item.find("hash");
    if (hash != item.end()) {
        if (!hash->is_string()) {
            return std::nullopt;
        }
        const auto parsed = parse_hex_hash(hash->get<std::string>());
        if (!parsed) {
            return std::nullopt;
        }
        ref.hash = *parsed;
    }
    return ref;
}

[[nodiscard]] bool parse_optional_layout_hash(const nlohmann::json& item,
                                              PortDescriptor& port) noexcept {
    const auto layout_hash = item.find("layout_hash");
    const auto contract_hash = item.find("contract_hash");
    if (layout_hash == item.end() && contract_hash == item.end()) {
        return true;
    }
    const auto& hash_field = (layout_hash != item.end()) ? layout_hash : contract_hash;
    if (!hash_field->is_string()) {
        return false;
    }
    const auto parsed = parse_hex_hash(hash_field->get<std::string>());
    if (!parsed) {
        return false;
    }
    port.layout_hash = *parsed;
    return true;
}

[[nodiscard]] std::optional<PortDescriptor> port_from_json(
    const nlohmann::json& item) noexcept {
    if (!item.is_object()) {
        return std::nullopt;
    }
    PortDescriptor port;
    if (!parse_required_string(item, "name", port.name)) {
        return std::nullopt;
    }
    const auto direction = item.find("direction");
    if (direction == item.end() || !direction->is_string() ||
        !parse_direction(direction->get<std::string>(), port.direction)) {
        return std::nullopt;
    }
    if (!parse_required_string(item, "semantics", port.semantics) ||
        !parse_required_string(item, "contract", port.contract_name)) {
        return std::nullopt;
    }
    const auto contract_version = item.find("contract_version");
    if (contract_version == item.end() ||
        !parse_version_json(*contract_version, port.contract_version)) {
        return std::nullopt;
    }
    if (!parse_optional_layout_hash(item, port)) {
        return std::nullopt;
    }
    const auto required = item.find("required");
    port.required =
        (required != item.end() && required->is_boolean() && required->get<bool>());
    return port;
}

[[nodiscard]] std::optional<ParameterDescriptor> parameter_from_json(
    const nlohmann::json& item) noexcept {
    if (!item.is_object()) {
        return std::nullopt;
    }
    ParameterDescriptor param;
    if (!parse_required_string(item, "name", param.name) ||
        !parse_required_string(item, "type", param.type)) {
        return std::nullopt;
    }
    const auto min = item.find("min");
    if (min != item.end()) {
        if (!min->is_number_integer() && !min->is_number_unsigned()) {
            return std::nullopt;
        }
        param.min = min->get<std::int64_t>();
    }
    const auto max = item.find("max");
    if (max != item.end()) {
        if (!max->is_number_integer() && !max->is_number_unsigned()) {
            return std::nullopt;
        }
        param.max = max->get<std::int64_t>();
    }
    const auto file = item.find("file");
    param.file = (file != item.end() && file->is_boolean() && file->get<bool>());
    const auto required = item.find("required");
    param.required =
        (required != item.end() && required->is_boolean() && required->get<bool>());
    return param;
}

[[nodiscard]] std::optional<ThreadRoleDescriptor> thread_role_from_json(
    const nlohmann::json& item) noexcept {
    if (!item.is_object()) {
        return std::nullopt;
    }
    ThreadRoleDescriptor role;
    if (!parse_required_string(item, "name", role.name)) {
        return std::nullopt;
    }
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
    return role;
}

[[nodiscard]] bool parse_metadata(const nlohmann::json& j,
                                  ComponentDescriptor& desc) noexcept {
    if (!parse_required_string(j, "name", desc.name)) {
        return false;
    }
    const auto version = j.find("version");
    return version != j.end() && parse_version_json(*version, desc.version);
}

[[nodiscard]] bool parse_sdk_version(const nlohmann::json& j,
                                     ComponentDescriptor& desc) noexcept {
    const nlohmann::json* sdk_obj = nullptr;
    if (!parse_required_object(j, "sdk", sdk_obj)) {
        return false;
    }
    const auto abi_major = sdk_obj->find("abi_major");
    const auto abi_minor = sdk_obj->find("abi_minor");
    if (abi_major == sdk_obj->end() || !abi_major->is_number_unsigned() ||
        abi_minor == sdk_obj->end() || !abi_minor->is_number_unsigned()) {
        return false;
    }
    desc.sdk.abi_major = abi_major->get<std::uint64_t>();
    desc.sdk.abi_minor = abi_minor->get<std::uint64_t>();
    return true;
}

[[nodiscard]] bool parse_contracts(const nlohmann::json& j,
                                   std::vector<ContractRef>& out) noexcept {
    return parse_array(j, out, contract_from_json);
}

[[nodiscard]] bool parse_ports(const nlohmann::json& j,
                               std::vector<PortDescriptor>& out) noexcept {
    return parse_array(j, out, port_from_json);
}

[[nodiscard]] bool parse_parameters(const nlohmann::json& j,
                                    std::vector<ParameterDescriptor>& out) noexcept {
    return parse_array(j, out, parameter_from_json);
}

[[nodiscard]] bool parse_thread_roles(const nlohmann::json& j,
                                      std::vector<ThreadRoleDescriptor>& out) noexcept {
    return parse_array(j, out, thread_role_from_json);
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

[[nodiscard]] bool parse_required_section(const nlohmann::json& obj,
                                          std::string_view key,
                                          auto& out,
                                          const auto& parse) noexcept {
    const auto it = obj.find(key);
    if (it == obj.end() || !parse(*it, out)) {
        return false;
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
    if (!parse_metadata(j, desc) ||
        !parse_sdk_version(j, desc) ||
        !parse_required_section(j, "contracts", desc.contracts, parse_contracts) ||
        !parse_required_section(j, "ports", desc.ports, parse_ports) ||
        !parse_required_section(j, "parameters", desc.parameters, parse_parameters) ||
        !parse_required_section(j, "thread_roles", desc.thread_roles, parse_thread_roles) ||
        !parse_required_section(j, "resources", desc.resources, parse_resources) ||
        !parse_required_section(j, "supported_modes", desc.supported_modes, parse_supported_modes)) {
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
