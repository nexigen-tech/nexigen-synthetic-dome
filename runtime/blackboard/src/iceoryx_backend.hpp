#pragma once

#include "backend.hpp"

namespace nxd::runtime::blackboard {

/// Multi-process Blackboard backend backed by Eclipse iceoryx2.
class IceoryxBackend final : public Backend {
  public:
    IceoryxBackend() noexcept;
    ~IceoryxBackend() override;

    int initialize() noexcept override;

    NxdBlackboardEntry* create_entry(std::string_view name,
                                     std::size_t value_size,
                                     const std::array<std::uint8_t, 32>& layout_hash) noexcept override;

    const NxdBlackboardEntry* open_entry(std::string_view name,
                                         const std::array<std::uint8_t, 32>& layout_hash) const noexcept override;

    std::int64_t write(NxdBlackboardEntry* entry,
                       const void* value,
                       std::size_t value_size,
                       std::uint64_t apply_frame_no) noexcept override;

    int read(const NxdBlackboardEntry* entry,
             void* out_value,
             std::size_t value_size,
             std::uint64_t* out_revision,
             std::uint64_t* out_apply_frame_no) const noexcept override;

    int get_update(const NxdBlackboardEntry* entry,
                   NxdBlackboardUpdate& out_update) const noexcept override;

    std::size_t entry_count() const noexcept override;
    const char* entry_name(std::size_t index) const noexcept override;
  private:
    struct EntryHandle;
    struct Impl;
    Impl* impl_ = nullptr;

    void reset() noexcept;
    static bool refresh_reader_cache(EntryHandle* handle) noexcept;
};

} // namespace nxd::runtime::blackboard
