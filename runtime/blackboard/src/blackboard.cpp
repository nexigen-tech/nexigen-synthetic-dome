#include "nxd/runtime/blackboard/blackboard.hpp"

#include <algorithm>
#include <cstring>

namespace nxd::runtime::blackboard {

namespace {

void copy_name(char* dst, std::size_t dst_size, std::string_view src) noexcept {
    if (dst_size == 0) {
        return;
    }
    std::size_t len = std::min(src.size(), dst_size - 1);
    std::memcpy(dst, src.data(), len);
    dst[len] = '\0';
}

bool layout_hash_equal(const std::array<std::uint8_t, 32>& a,
                       const std::array<std::uint8_t, 32>& b) noexcept {
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

} // namespace

Entry::Entry(std::string_view name,
             std::size_t value_size,
             const std::array<std::uint8_t, 32>& layout_hash)
    : layout_hash_(layout_hash), value_(value_size) {
    copy_name(name_.data(), name_.size(), name);
}

std::int64_t Entry::write(const void* value, std::size_t len, std::uint64_t apply_frame_no) noexcept {
    if (value == nullptr || len != value_.size()) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    const std::uint64_t old_revision = revision_.load(std::memory_order_relaxed);
    revision_.store(old_revision + 1, std::memory_order_release);

    std::memcpy(value_.data(), value, len);
    apply_frame_no_.store(apply_frame_no, std::memory_order_relaxed);
    generation_.store(generation_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

    // Update the snapshot event using a separate sequence lock.
    const std::uint64_t old_update = update_seq_.load(std::memory_order_relaxed);
    update_seq_.store(old_update + 1, std::memory_order_release);
    copy_name(update_.entry_name, sizeof(update_.entry_name), name());
    std::memcpy(update_.layout_hash, layout_hash_.data(), layout_hash_.size());
    update_.revision = old_revision + 2;
    update_.apply_frame_no = apply_frame_no;
    update_.generation = generation_.load(std::memory_order_relaxed);
    update_seq_.store(old_update + 2, std::memory_order_release);

    revision_.store(old_revision + 2, std::memory_order_release);
    return static_cast<std::int64_t>(old_revision + 2);
}

int Entry::read(void* out, std::size_t len, std::uint64_t& revision, std::uint64_t& apply_frame_no) const noexcept {
    if (out == nullptr || len != value_.size()) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    for (;;) {
        std::uint64_t r1 = revision_.load(std::memory_order_acquire);
        if ((r1 & 1u) != 0) {
            continue;
        }
        std::memcpy(out, value_.data(), len);
        std::uint64_t app = apply_frame_no_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        std::uint64_t r2 = revision_.load(std::memory_order_acquire);
        if (r1 == r2) {
            revision = r1;
            apply_frame_no = app;
            return NXD_BLACKBOARD_OK;
        }
    }
}

int Entry::get_update(NxdBlackboardUpdate& out) const noexcept {
    for (;;) {
        std::uint64_t s1 = update_seq_.load(std::memory_order_acquire);
        if ((s1 & 1u) != 0) {
            continue;
        }
        std::memcpy(&out, &update_, sizeof(out));
        std::atomic_thread_fence(std::memory_order_acquire);
        std::uint64_t s2 = update_seq_.load(std::memory_order_acquire);
        if (s1 == s2) {
            return NXD_BLACKBOARD_OK;
        }
    }
}

Blackboard& Blackboard::instance() noexcept {
    static Blackboard instance;
    return instance;
}

Entry* Blackboard::create_entry(std::string_view name,
                                std::size_t value_size,
                                const std::array<std::uint8_t, 32>& layout_hash) noexcept {
    if (name.empty() || value_size == 0) {
        return nullptr;
    }
    std::lock_guard lock(mutex_);
    for (const auto& slot : slots_) {
        if (slot.entry && slot.entry->name() == name) {
            return nullptr;
        }
    }
    for (auto& slot : slots_) {
        if (!slot.entry) {
            slot.entry = std::unique_ptr<Entry>(new (std::nothrow) Entry(name, value_size, layout_hash));
            return slot.entry.get();
        }
    }
    return nullptr;
}

const Entry* Blackboard::open_entry(std::string_view name,
                                    const std::array<std::uint8_t, 32>& layout_hash) const noexcept {
    if (name.empty()) {
        return nullptr;
    }
    std::lock_guard lock(mutex_);
    for (const auto& slot : slots_) {
        if (slot.entry && slot.entry->name() == name) {
            if (!layout_hash_equal(slot.entry->layout_hash(), layout_hash)) {
                return nullptr;
            }
            return slot.entry.get();
        }
    }
    return nullptr;
}

} // namespace nxd::runtime::blackboard

extern "C" {

int nxd_blackboard_initialize(void) {
    nxd::runtime::blackboard::Blackboard::instance();
    return NXD_BLACKBOARD_OK;
}

NxdBlackboardEntry* nxd_blackboard_create_entry(const char* name,
                                                std::uint64_t value_size,
                                                const std::uint8_t layout_hash[32]) {
    if (name == nullptr || layout_hash == nullptr) {
        return nullptr;
    }
    std::array<std::uint8_t, 32> hash{};
    std::memcpy(hash.data(), layout_hash, hash.size());
    auto* entry = nxd::runtime::blackboard::Blackboard::instance().create_entry(
        std::string_view(name), static_cast<std::size_t>(value_size), hash);
    return reinterpret_cast<NxdBlackboardEntry*>(entry);
}

const NxdBlackboardEntry* nxd_blackboard_open_entry(const char* name,
                                                    const std::uint8_t layout_hash[32]) {
    if (name == nullptr || layout_hash == nullptr) {
        return nullptr;
    }
    std::array<std::uint8_t, 32> hash{};
    std::memcpy(hash.data(), layout_hash, hash.size());
    const auto* entry = nxd::runtime::blackboard::Blackboard::instance().open_entry(
        std::string_view(name), hash);
    return reinterpret_cast<const NxdBlackboardEntry*>(entry);
}

std::int64_t nxd_blackboard_write(NxdBlackboardEntry* entry,
                                  const void* value,
                                  std::uint64_t value_size,
                                  std::uint64_t apply_frame_no) {
    if (entry == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }
    auto* e = reinterpret_cast<nxd::runtime::blackboard::Entry*>(entry);
    return e->write(value, static_cast<std::size_t>(value_size), apply_frame_no);
}

int nxd_blackboard_read(const NxdBlackboardEntry* entry,
                        void* out_value,
                        std::uint64_t value_size,
                        std::uint64_t* out_revision,
                        std::uint64_t* out_apply_frame_no) {
    if (entry == nullptr || out_revision == nullptr || out_apply_frame_no == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }
    const auto* e = reinterpret_cast<const nxd::runtime::blackboard::Entry*>(entry);
    std::uint64_t revision = 0;
    std::uint64_t apply_frame_no = 0;
    int rc = e->read(out_value, static_cast<std::size_t>(value_size), revision, apply_frame_no);
    if (rc == NXD_BLACKBOARD_OK) {
        *out_revision = revision;
        *out_apply_frame_no = apply_frame_no;
    }
    return rc;
}

int nxd_blackboard_get_update(const NxdBlackboardEntry* entry, NxdBlackboardUpdate* out_update) {
    if (entry == nullptr || out_update == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }
    const auto* e = reinterpret_cast<const nxd::runtime::blackboard::Entry*>(entry);
    return e->get_update(*out_update);
}

} // extern "C"
