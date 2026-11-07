#include "iceoryx_backend.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "iox2/iceoryx2.h"

namespace nxd::runtime::blackboard {

namespace {

constexpr std::size_t kMaxEntries = 16;
// Fixed payload size shared by all entries.  The actual value_size is stored
// in the sample user header so callers still read exactly value_size bytes.
constexpr std::size_t kMaxPayloadSize = 1024;
constexpr std::size_t payload_alignment = alignof(std::max_align_t);

struct BlackboardSampleHeader {
    std::uint64_t revision = 0;
    std::uint64_t apply_frame_no = 0;
    std::uint64_t generation = 0;
    std::uint64_t value_size = 0;
    std::uint8_t layout_hash[32] = {};
};

std::string layout_hash_string(const std::array<std::uint8_t, 32>& hash) noexcept {
    std::string out;
    out.reserve(64);
    char buffer[3];
    for (std::uint8_t byte : hash) {
        std::snprintf(buffer, sizeof(buffer), "%02x", byte);
        out += buffer[0];
        out += buffer[1];
    }
    return out;
}

std::string entry_service_name(std::string_view entry_name,
                               const std::array<std::uint8_t, 32>& layout_hash) noexcept {
    std::string sanitized;
    sanitized.reserve(entry_name.size());
    for (char c : entry_name) {
        sanitized += (c == '/') ? '_' : c;
    }
    return std::string("nxd_bb/entry/") + layout_hash_string(layout_hash) + "/" + sanitized;
}

std::string writer_lock_path(std::string_view entry_name) {
    std::string path = "/tmp/opencode/nxd_bb_lock/";
    for (char c : entry_name) {
        path += (c == '/') ? '_' : c;
    }
    return path + ".lock";
}

void copy_name(char* dst, std::size_t dst_size, std::string_view src) noexcept {
    if (dst_size == 0) {
        return;
    }
    std::size_t len = std::min(src.size(), dst_size - 1);
    std::memcpy(dst, src.data(), len);
    dst[len] = '\0';
}

bool ensure_lock_directory() noexcept {
    constexpr const char* kLockDir = "/tmp/opencode/nxd_bb_lock";
    if (mkdir(kLockDir, 0777) == 0 || errno == EEXIST) {
        return true;
    }
    if (errno == ENOENT) {
        if (mkdir("/tmp/opencode", 0777) != 0 && errno != EEXIST) {
            return false;
        }
        if (mkdir(kLockDir, 0777) == 0 || errno == EEXIST) {
            return true;
        }
    }
    return false;
}

// Acquires an exclusive per-entry writer lock.  Returns the file descriptor on
// success; the lock is released when the descriptor is closed.
bool acquire_writer_lock(std::string_view entry_name, int* out_fd) noexcept {
    if (!ensure_lock_directory()) {
        return false;
    }

    const std::string path = writer_lock_path(entry_name);
    const int fd = open(path.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        return false;
    }

    struct flock fl{};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLK, &fl) < 0) {
        close(fd);
        return false;
    }

    *out_fd = fd;
    return true;
}

} // namespace

struct IceoryxBackend::EntryHandle {
    std::string name;
    std::size_t value_size = 0;
    std::array<std::uint8_t, 32> layout_hash{};
    bool is_writer = false;
    pid_t owner_pid = -1;
    bool drop_resources = true;
    int lock_fd = -1;

    iox2_publisher_h publisher = nullptr;
    iox2_subscriber_h subscriber = nullptr;

    // Writer-side sequence counters and cached value.
    std::uint64_t revision = 0;
    std::uint64_t generation = 0;
    std::uint64_t last_apply_frame_no = 0;
    std::array<std::byte, kMaxPayloadSize> writer_value{};
    std::size_t writer_value_size = 0;

    // Reader-side cache of the latest received sample.
    std::vector<std::byte> read_buffer;
    std::uint64_t last_revision = 0;
    std::uint64_t last_apply_frame_no_reader = 0;
    std::uint64_t last_generation = 0;
    bool has_sample = false;

    ~EntryHandle() {
        if (lock_fd >= 0) {
            close(lock_fd);
        }
        if (!drop_resources) {
            return;
        }
        if (publisher != nullptr) {
            iox2_publisher_drop(publisher);
        }
        if (subscriber != nullptr) {
            iox2_subscriber_drop(subscriber);
        }
    }
};

struct IceoryxBackend::Impl {
    iox2_node_h node = nullptr;
    pid_t node_pid = -1;
    std::unordered_map<std::string, std::unique_ptr<EntryHandle>> entries;
};

IceoryxBackend::IceoryxBackend() noexcept : impl_(new (std::nothrow) Impl{}) {
    if (impl_ != nullptr) {
        impl_->node = nullptr;
        impl_->node_pid = -1;
    }
}

IceoryxBackend::~IceoryxBackend() {
    if (impl_ == nullptr) {
        return;
    }
    reset();
    delete impl_;
}

void IceoryxBackend::reset() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->entries.clear();
    if (impl_->node != nullptr) {
        iox2_node_drop(impl_->node);
        impl_->node = nullptr;
    }
    impl_->node_pid = -1;
}

int IceoryxBackend::initialize() noexcept {
    if (impl_ == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    // After a fork the COW copy of the global backend state still references
    // resources owned by the parent process.  Abandon those references without
    // dropping them (dropping in the child would release the parent's ports)
    // and build a fresh node for this process.
    if (impl_->node != nullptr && impl_->node_pid != ::getpid()) {
        for (auto& [name, handle] : impl_->entries) {
            if (handle->owner_pid != ::getpid()) {
                handle->drop_resources = false;
            }
        }
        impl_->entries.clear();
        impl_->node = nullptr;
        impl_->node_pid = -1;
    }

    if (impl_->node != nullptr) {
        return NXD_BLACKBOARD_OK;
    }

    iox2_set_log_level_from_env_or(iox2_log_level_e_WARN);

    iox2_node_builder_h builder = iox2_node_builder_new(nullptr);
    if (builder == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    if (iox2_node_builder_create(builder, nullptr, iox2_service_type_e_IPC, &impl_->node) != IOX2_OK) {
        impl_->node = nullptr;
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    impl_->node_pid = ::getpid();
    return NXD_BLACKBOARD_OK;
}

namespace {

bool open_value_service(iox2_node_h node,
                        std::string_view service_name,
                        bool create_if_missing,
                        iox2_port_factory_pub_sub_h* out_factory) noexcept {
    *out_factory = nullptr;

    iox2_service_name_h service_name_h = nullptr;
    if (iox2_service_name_new(nullptr, service_name.data(), service_name.size(), &service_name_h) != IOX2_OK) {
        return false;
    }

    iox2_service_builder_h service_builder =
        iox2_node_service_builder(&node, nullptr, iox2_cast_service_name_ptr(service_name_h));
    iox2_service_builder_pub_sub_h ps_builder = iox2_service_builder_pub_sub(service_builder);

    if (iox2_service_builder_pub_sub_set_payload_type_details(&ps_builder,
                                                              iox2_type_variant_e_FIXED_SIZE,
                                                              "NxdBlackboardValue",
                                                              std::strlen("NxdBlackboardValue"),
                                                              kMaxPayloadSize,
                                                              payload_alignment) != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return false;
    }

    if (iox2_service_builder_pub_sub_set_user_header_type_details(&ps_builder,
                                                                  iox2_type_variant_e_FIXED_SIZE,
                                                                  "NxdBlackboardHeader",
                                                                  std::strlen("NxdBlackboardHeader"),
                                                                  sizeof(BlackboardSampleHeader),
                                                                  alignof(BlackboardSampleHeader)) != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return false;
    }

    const int rc = create_if_missing
                       ? iox2_service_builder_pub_sub_open_or_create(ps_builder, nullptr, out_factory)
                       : iox2_service_builder_pub_sub_open(ps_builder, nullptr, out_factory);
    iox2_service_name_drop(service_name_h);

    return rc == IOX2_OK && *out_factory != nullptr;
}

bool send_sample(iox2_publisher_h publisher,
                 const BlackboardSampleHeader& header,
                 const void* value,
                 std::size_t value_size) noexcept {
    if (publisher == nullptr) {
        return false;
    }

    iox2_sample_mut_h sample = nullptr;
    if (iox2_publisher_loan_slice_uninit(&publisher, nullptr, &sample, 1) != IOX2_OK || sample == nullptr) {
        return false;
    }

    void* payload = nullptr;
    std::size_t count = 0;
    iox2_sample_mut_payload_mut(&sample, &payload, &count);

    void* header_ptr = nullptr;
    iox2_sample_mut_user_header_mut(&sample, &header_ptr);

    if (payload != nullptr && count == 1) {
        std::memset(payload, 0, kMaxPayloadSize);
        if (value != nullptr && value_size > 0) {
            std::memcpy(payload, value, value_size);
        }
    }

    if (header_ptr != nullptr) {
        std::memcpy(header_ptr, &header, sizeof(header));
    }

    iox2_publisher_update_connections(&publisher);
    iox2_sample_mut_send(sample, nullptr);
    return true;
}

} // namespace

NxdBlackboardEntry* IceoryxBackend::create_entry(std::string_view name,
                                                 std::size_t value_size,
                                                 const std::array<std::uint8_t, 32>& layout_hash) noexcept {
    if (impl_ == nullptr || impl_->node == nullptr || name.empty() || value_size == 0 || value_size > kMaxPayloadSize ||
        impl_->entries.size() >= kMaxEntries) {
        return nullptr;
    }

    const std::string key(name);
    if (impl_->entries.contains(key)) {
        return nullptr;
    }

    // Step 1: acquire the exclusive per-entry writer lock.  This enforces
    // single-writer semantics even though iceoryx2 publishers are not limited
    // to one in this configuration.
    int lock_fd = -1;
    if (!acquire_writer_lock(name, &lock_fd)) {
        return nullptr;
    }

    iox2_port_factory_pub_sub_h factory = nullptr;
    if (!open_value_service(impl_->node, entry_service_name(name, layout_hash), true, &factory)) {
        close(lock_fd);
        return nullptr;
    }

    iox2_port_factory_publisher_builder_h pub_builder =
        iox2_port_factory_pub_sub_publisher_builder(&factory, nullptr);
    iox2_publisher_h publisher_h = nullptr;
    const int pub_rc = iox2_port_factory_publisher_builder_create(pub_builder, nullptr, &publisher_h);
    iox2_port_factory_pub_sub_drop(factory);

    if (pub_rc != IOX2_OK || publisher_h == nullptr) {
        close(lock_fd);
        return nullptr;
    }

    auto handle = std::unique_ptr<EntryHandle>(new (std::nothrow) EntryHandle{});
    if (handle == nullptr) {
        iox2_publisher_drop(publisher_h);
        close(lock_fd);
        return nullptr;
    }

    handle->name = key;
    handle->value_size = value_size;
    handle->layout_hash = layout_hash;
    handle->is_writer = true;
    handle->owner_pid = ::getpid();
    handle->lock_fd = lock_fd;
    handle->publisher = publisher_h;

    NxdBlackboardEntry* opaque = reinterpret_cast<NxdBlackboardEntry*>(handle.get());
    impl_->entries.emplace(key, std::move(handle));
    return opaque;
}

const NxdBlackboardEntry* IceoryxBackend::open_entry(std::string_view name,
                                                     const std::array<std::uint8_t, 32>& layout_hash) const noexcept {
    auto* mutable_this = const_cast<IceoryxBackend*>(this);
    if (mutable_this == nullptr || mutable_this->impl_ == nullptr || mutable_this->impl_->node == nullptr || name.empty() ||
        mutable_this->impl_->entries.size() >= kMaxEntries) {
        return nullptr;
    }

    const std::string key(name);

    // Use `open` (not `open_or_create`) so that opening with a mismatched
    // layout hash fails: the service name includes the hash, and a writer with
    // a different hash would have created a different service.
    iox2_port_factory_pub_sub_h factory = nullptr;
    if (!open_value_service(mutable_this->impl_->node, entry_service_name(name, layout_hash), false, &factory)) {
        return nullptr;
    }

    iox2_port_factory_subscriber_builder_h sub_builder =
        iox2_port_factory_pub_sub_subscriber_builder(&factory, nullptr);
    iox2_subscriber_h subscriber_h = nullptr;
    const int sub_rc = iox2_port_factory_subscriber_builder_create(sub_builder, nullptr, &subscriber_h);
    iox2_port_factory_pub_sub_drop(factory);

    if (sub_rc != IOX2_OK || subscriber_h == nullptr) {
        return nullptr;
    }

    auto handle = std::unique_ptr<EntryHandle>(new (std::nothrow) EntryHandle{});
    if (handle == nullptr) {
        iox2_subscriber_drop(subscriber_h);
        return nullptr;
    }

    handle->name = key;
    handle->value_size = 0; // Discovered from the first sample header.
    handle->layout_hash = layout_hash;
    handle->is_writer = false;
    handle->owner_pid = ::getpid();
    handle->subscriber = subscriber_h;

    const NxdBlackboardEntry* opaque = reinterpret_cast<const NxdBlackboardEntry*>(handle.get());
    mutable_this->impl_->entries.emplace(key, std::move(handle));
    return opaque;
}

std::int64_t IceoryxBackend::write(NxdBlackboardEntry* entry,
                                   const void* value,
                                   std::size_t value_size,
                                   std::uint64_t apply_frame_no) noexcept {
    if (impl_ == nullptr || entry == nullptr || value == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    auto* handle = reinterpret_cast<EntryHandle*>(entry);
    if (!handle->is_writer || handle->publisher == nullptr || value_size != handle->value_size ||
        value_size > kMaxPayloadSize) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    handle->revision += 1;
    handle->generation += 1;
    handle->last_apply_frame_no = apply_frame_no;

    BlackboardSampleHeader header{};
    header.revision = handle->revision;
    header.apply_frame_no = apply_frame_no;
    header.generation = handle->generation;
    header.value_size = static_cast<std::uint64_t>(value_size);
    std::memcpy(header.layout_hash, handle->layout_hash.data(), handle->layout_hash.size());

    if (!send_sample(handle->publisher, header, value, value_size)) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    handle->writer_value_size = value_size;
    std::memcpy(handle->writer_value.data(), value, value_size);

    return static_cast<std::int64_t>(handle->revision);
}

bool IceoryxBackend::refresh_reader_cache(EntryHandle* handle) noexcept {
    if (handle == nullptr || handle->subscriber == nullptr) {
        return false;
    }

    iox2_sample_h sample = nullptr;
    if (iox2_subscriber_receive(&handle->subscriber, nullptr, &sample) != IOX2_OK || sample == nullptr) {
        return handle->has_sample;
    }

    const void* payload = nullptr;
    std::size_t count = 0;
    iox2_sample_payload(&sample, &payload, &count);

    const void* header_ptr = nullptr;
    iox2_sample_user_header(&sample, &header_ptr);

    if (payload != nullptr && count == 1 && header_ptr != nullptr) {
        const auto& header = *static_cast<const BlackboardSampleHeader*>(header_ptr);
        if (std::memcmp(handle->layout_hash.data(), header.layout_hash, handle->layout_hash.size()) != 0) {
            iox2_sample_drop(sample);
            return handle->has_sample;
        }

        if (header.revision != 0) {
            const std::size_t sample_bytes = iox2_sample_payload_number_of_bytes(&sample);
            const std::size_t bytes = std::min(sample_bytes, kMaxPayloadSize);
            handle->read_buffer.assign(static_cast<const std::byte*>(payload),
                                       static_cast<const std::byte*>(payload) + bytes);
            handle->last_revision = header.revision;
            handle->last_apply_frame_no_reader = header.apply_frame_no;
            handle->last_generation = header.generation;
            handle->value_size = static_cast<std::size_t>(header.value_size);
            handle->has_sample = true;
        }
    }

    iox2_sample_drop(sample);
    return handle->has_sample;
}

int IceoryxBackend::read(const NxdBlackboardEntry* entry,
                         void* out_value,
                         std::size_t value_size,
                         std::uint64_t* out_revision,
                         std::uint64_t* out_apply_frame_no) const noexcept {
    if (impl_ == nullptr || entry == nullptr || out_value == nullptr || out_revision == nullptr ||
        out_apply_frame_no == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    auto* handle = reinterpret_cast<EntryHandle*>(const_cast<NxdBlackboardEntry*>(entry));

    if (handle->is_writer) {
        if (value_size != handle->writer_value_size) {
            return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
        }
        std::memcpy(out_value, handle->writer_value.data(), value_size);
        *out_revision = handle->revision;
        *out_apply_frame_no = handle->last_apply_frame_no;
        return NXD_BLACKBOARD_OK;
    }

    if (!refresh_reader_cache(handle)) {
        return NXD_BLACKBOARD_ERROR_NOT_FOUND;
    }

    if (value_size != handle->value_size) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    std::memcpy(out_value, handle->read_buffer.data(), value_size);
    *out_revision = handle->last_revision;
    *out_apply_frame_no = handle->last_apply_frame_no_reader;
    return NXD_BLACKBOARD_OK;
}

int IceoryxBackend::get_update(const NxdBlackboardEntry* entry, NxdBlackboardUpdate& out_update) const noexcept {
    if (impl_ == nullptr || entry == nullptr) {
        return NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT;
    }

    auto* handle = reinterpret_cast<EntryHandle*>(const_cast<NxdBlackboardEntry*>(entry));
    if (handle->is_writer) {
        copy_name(out_update.entry_name, sizeof(out_update.entry_name), handle->name);
        std::memcpy(out_update.layout_hash, handle->layout_hash.data(), handle->layout_hash.size());
        out_update.revision = handle->revision;
        out_update.apply_frame_no = handle->last_apply_frame_no;
        out_update.generation = handle->generation;
        return NXD_BLACKBOARD_OK;
    }

    if (!refresh_reader_cache(handle)) {
        return NXD_BLACKBOARD_ERROR_NOT_FOUND;
    }

    copy_name(out_update.entry_name, sizeof(out_update.entry_name), handle->name);
    std::memcpy(out_update.layout_hash, handle->layout_hash.data(), handle->layout_hash.size());
    out_update.revision = handle->last_revision;
    out_update.apply_frame_no = handle->last_apply_frame_no_reader;
    out_update.generation = handle->last_generation;
    return NXD_BLACKBOARD_OK;
}

std::size_t IceoryxBackend::entry_count() const noexcept {
    if (impl_ == nullptr) {
        return 0;
    }
    return impl_->entries.size();
}

const char* IceoryxBackend::entry_name(std::size_t index) const noexcept {
    if (impl_ == nullptr) {
        return nullptr;
    }
    std::size_t current = 0;
    for (const auto& [key, handle] : impl_->entries) {
        if (current == index) {
            return handle->name.c_str();
        }
        ++current;
    }
    return nullptr;
}

} // namespace nxd::runtime::blackboard
