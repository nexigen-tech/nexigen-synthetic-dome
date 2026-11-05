#include "nxd/runtime/chronos/partition.hpp"

#include <atomic>
#include <cstdint>
#include <utility>

namespace nxd::runtime::chronos {

Partition::Partition(std::uint32_t id, std::string name) noexcept
    : id_(id), name_(std::move(name)) {}

std::uint32_t Partition::id() const noexcept {
    return id_;
}

const std::string& Partition::name() const noexcept {
    return name_;
}

void Partition::publish(const FrameContext& ctx) noexcept {
    // Mark the snapshot as being written (odd sequence number).
    sequence_.fetch_add(1, std::memory_order_acq_rel);

    snapshot_ = ctx;

    // Mark the snapshot as consistent (even sequence number).
    sequence_.fetch_add(1, std::memory_order_release);
}

FrameContext Partition::latest() const noexcept {
    FrameContext ctx{};
    std::uint64_t seq = 0;

    do {
        seq = sequence_.load(std::memory_order_acquire);
        // Spin while a writer is in the middle of publishing.
        while (seq % 2 != 0) {
            seq = sequence_.load(std::memory_order_acquire);
        }

        ctx = snapshot_;
    } while (sequence_.load(std::memory_order_acquire) != seq);

    return ctx;
}

} // namespace nxd::runtime::chronos
