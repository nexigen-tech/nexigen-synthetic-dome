#pragma once

#include "nxd/runtime/blackboard/blackboard_c_api.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace nxd::runtime::blackboard {

class Entry {
  public:
    Entry(std::string_view name, std::size_t value_size, const std::array<std::uint8_t, 32>& layout_hash);

    [[nodiscard]] std::int64_t write(const void* value, std::size_t len, std::uint64_t apply_frame_no) noexcept;
    [[nodiscard]] int read(void* out, std::size_t len, std::uint64_t& revision, std::uint64_t& apply_frame_no) const noexcept;
    [[nodiscard]] int get_update(NxdBlackboardUpdate& out) const noexcept;

    [[nodiscard]] std::string_view name() const noexcept { return std::string_view(name_.data()); }
    [[nodiscard]] const std::array<std::uint8_t, 32>& layout_hash() const noexcept { return layout_hash_; }
    [[nodiscard]] std::size_t value_size() const noexcept { return value_.size(); }

  private:
    std::array<char, 64> name_{};
    std::array<std::uint8_t, 32> layout_hash_{};
    std::vector<std::byte> value_;
    alignas(64) mutable std::atomic<std::uint64_t> revision_{0};
    std::atomic<std::uint64_t> apply_frame_no_{0};
    std::atomic<std::uint64_t> generation_{0};
    mutable std::atomic<std::uint64_t> update_seq_{0};
    mutable NxdBlackboardUpdate update_{};
};

class Blackboard {
  public:
    static constexpr std::size_t max_entries = 16;

    static Blackboard& instance() noexcept;

    [[nodiscard]] Entry* create_entry(std::string_view name,
                                      std::size_t value_size,
                                      const std::array<std::uint8_t, 32>& layout_hash) noexcept;
    [[nodiscard]] const Entry* open_entry(std::string_view name,
                                          const std::array<std::uint8_t, 32>& layout_hash) const noexcept;

  private:
    mutable std::mutex mutex_;
    struct Slot {
        std::unique_ptr<Entry> entry;
    };
    std::array<Slot, max_entries> slots_;
};

} // namespace nxd::runtime::blackboard
