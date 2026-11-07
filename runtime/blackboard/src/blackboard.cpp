#include "nxd/runtime/blackboard/blackboard.hpp"

#include "backend.hpp"
#include "iceoryx_backend.hpp"

#include <algorithm>
#include <cstdlib>
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

std::array<std::uint8_t, 32> copy_hash(const std::uint8_t layout_hash[32]) noexcept {
    std::array<std::uint8_t, 32> hash{};
    if (layout_hash != nullptr) {
        std::memcpy(hash.data(), layout_hash, hash.size());
    }
    return hash;
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

std::size_t Blackboard::entry_count() const noexcept {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (const auto& slot : slots_) {
        if (slot.entry) {
            ++count;
        }
    }
    return count;
}

const Entry* Blackboard::entry(std::size_t index) const noexcept {
    std::lock_guard lock(mutex_);
    std::size_t current = 0;
    for (const auto& slot : slots_) {
        if (slot.entry) {
            if (current == index) {
                return slot.entry.get();
            }
            ++current;
        }
    }
    return nullptr;
}

namespace {

/// Headless backend adapter.  Delegates to the in-process Blackboard singleton.
class HeadlessBackend final : public Backend {
  public:
    int initialize() noexcept override {
        Blackboard::instance();
        return NXD_BLACKBOARD_OK;
    }

    NxdBlackboardEntry* create_entry(std::string_view name,
                                     std::size_t value_size,
                                     const std::array<std::uint8_t, 32>& layout_hash) noexcept override {
        auto* entry = Blackboard::instance().create_entry(name, value_size, layout_hash);
        return reinterpret_cast<NxdBlackboardEntry*>(entry);
    }

    const NxdBlackboardEntry* open_entry(std::string_view name,
                                         const std::array<std::uint8_t, 32>& layout_hash) const noexcept override {
        const auto* entry = Blackboard::instance().open_entry(name, layout_hash);
        return reinterpret_cast<const NxdBlackboardEntry*>(entry);
    }

    std::int64_t write(NxdBlackboardEntry* entry,
                       const void* value,
                       std::size_t value_size,
                       std::uint64_t apply_frame_no) noexcept override {
        if (entry == nullptr) {
            return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
        }
        auto* e = reinterpret_cast<Entry*>(entry);
        return e->write(value, value_size, apply_frame_no);
    }

    int read(const NxdBlackboardEntry* entry,
             void* out_value,
             std::size_t value_size,
             std::uint64_t* out_revision,
             std::uint64_t* out_apply_frame_no) const noexcept override {
        if (entry == nullptr || out_revision == nullptr || out_apply_frame_no == nullptr) {
            return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
        }
        const auto* e = reinterpret_cast<const Entry*>(entry);
        return e->read(out_value, value_size, *out_revision, *out_apply_frame_no);
    }

    int get_update(const NxdBlackboardEntry* entry, NxdBlackboardUpdate& out_update) const noexcept override {
        if (entry == nullptr) {
            return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
        }
        const auto* e = reinterpret_cast<const Entry*>(entry);
        return e->get_update(out_update);
    }

    std::size_t entry_count() const noexcept override {
        return Blackboard::instance().entry_count();
    }

    const char* entry_name(std::size_t index) const noexcept override {
        const auto* e = Blackboard::instance().entry(index);
        if (e == nullptr) {
            return nullptr;
        }
        return e->name().data();
    }
};

bool use_iceoryx2_backend() noexcept {
    const char* env = std::getenv("NXD_BLACKBOARD_BACKEND");
    if (env == nullptr) {
        return false;
    }
    return std::strcmp(env, "iceoryx2") == 0;
}

Backend& backend() noexcept {
    static HeadlessBackend headless;
    static IceoryxBackend iceoryx2;
    // The iceoryx2 backend is instantiated lazily on first use so that
    // processes that do not need it never initialize iceoryx2.
    if (use_iceoryx2_backend()) {
        return iceoryx2;
    }
    return headless;
}

} // namespace

} // namespace nxd::runtime::blackboard

extern "C" {

int nxd_blackboard_initialize(void) {
    return nxd::runtime::blackboard::backend().initialize();
}

NxdBlackboardEntry* nxd_blackboard_create_entry(const char* name,
                                                std::uint64_t value_size,
                                                const std::uint8_t layout_hash[32]) {
    if (name == nullptr || layout_hash == nullptr || value_size == 0) {
        return nullptr;
    }
    auto hash = nxd::runtime::blackboard::copy_hash(layout_hash);
    return nxd::runtime::blackboard::backend().create_entry(
        std::string_view(name), static_cast<std::size_t>(value_size), hash);
}

const NxdBlackboardEntry* nxd_blackboard_open_entry(const char* name,
                                                    const std::uint8_t layout_hash[32]) {
    if (name == nullptr || layout_hash == nullptr) {
        return nullptr;
    }
    auto hash = nxd::runtime::blackboard::copy_hash(layout_hash);
    return nxd::runtime::blackboard::backend().open_entry(
        std::string_view(name), hash);
}

std::int64_t nxd_blackboard_write(NxdBlackboardEntry* entry,
                                  const void* value,
                                  std::uint64_t value_size,
                                  std::uint64_t apply_frame_no) {
    return nxd::runtime::blackboard::backend().write(
        entry, value, static_cast<std::size_t>(value_size), apply_frame_no);
}

int nxd_blackboard_read(const NxdBlackboardEntry* entry,
                        void* out_value,
                        std::uint64_t value_size,
                        std::uint64_t* out_revision,
                        std::uint64_t* out_apply_frame_no) {
    return nxd::runtime::blackboard::backend().read(
        entry, out_value, static_cast<std::size_t>(value_size), out_revision, out_apply_frame_no);
}

int nxd_blackboard_get_update(const NxdBlackboardEntry* entry, NxdBlackboardUpdate* out_update) {
    if (entry == nullptr || out_update == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }
    return nxd::runtime::blackboard::backend().get_update(entry, *out_update);
}

int nxd_blackboard_entry_count(void) {
    return static_cast<int>(nxd::runtime::blackboard::backend().entry_count());
}

int nxd_blackboard_get_entry_name(int index, char* out, std::size_t len) {
    if (index < 0 || out == nullptr || len == 0) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }
    const char* name = nxd::runtime::blackboard::backend().entry_name(
        static_cast<std::size_t>(index));
    if (name == nullptr) {
        return NXD_BLACKBOARD_ERROR_NOT_FOUND;
    }
    const std::size_t copy_len = std::min(std::strlen(name), len - 1);
    std::memcpy(out, name, copy_len);
    out[copy_len] = '\0';
    return NXD_BLACKBOARD_OK;
}

} // extern "C"
