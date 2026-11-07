#pragma once

#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nxd::runtime::blackboard {

/// Backend-agnostic interface used by the public C API.  Each backend
/// implements the same lifecycle, entry management, read/write, and update
/// semantics.
class Backend {
  public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual int initialize() noexcept = 0;

    [[nodiscard]] virtual NxdBlackboardEntry* create_entry(
        std::string_view name,
        std::size_t value_size,
        const std::array<std::uint8_t, 32>& layout_hash) noexcept = 0;

    [[nodiscard]] virtual const NxdBlackboardEntry* open_entry(
        std::string_view name,
        const std::array<std::uint8_t, 32>& layout_hash) const noexcept = 0;

    [[nodiscard]] virtual std::int64_t write(NxdBlackboardEntry* entry,
                                             const void* value,
                                             std::size_t value_size,
                                             std::uint64_t apply_frame_no) noexcept = 0;

    [[nodiscard]] virtual int read(const NxdBlackboardEntry* entry,
                                   void* out_value,
                                   std::size_t value_size,
                                   std::uint64_t* out_revision,
                                   std::uint64_t* out_apply_frame_no) const noexcept = 0;

    [[nodiscard]] virtual int get_update(const NxdBlackboardEntry* entry,
                                         NxdBlackboardUpdate& out_update) const noexcept = 0;

    [[nodiscard]] virtual std::size_t entry_count() const noexcept = 0;

    [[nodiscard]] virtual const char* entry_name(std::size_t index) const noexcept = 0;
};

} // namespace nxd::runtime::blackboard
