/* NXD plugin SDK C++ wrapper.
 *
 * Thin, zero-allocation wrappers around the ABI-1 C types.  This header is
 * valid under C++23 with -fno-exceptions and -fno-rtti.  Nothing in this
 * header accesses wall-clock time, physical channels, CPU affinity, or
 * transport endpoints.
 */

#ifndef NXD_SDK_HPP
#define NXD_SDK_HPP

#include "nxd/abi.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace nxd {

/// Deterministic RNG exposed through the frame context.
class Rng {
  public:
    using Callback = std::uint64_t (*)(void* state);

    constexpr Rng() noexcept = default;

    /// Wraps the RNG callback from an ABI frame context.
    explicit Rng(const NxdFrameContext& ctx) noexcept
        : callback_(reinterpret_cast<Callback>(ctx.rng)), state_(ctx.rng_state) {}

    /// Returns true if the host armed a deterministic RNG.
    [[nodiscard]] constexpr bool armed() const noexcept {
        return callback_ != nullptr;
    }

    /// Returns the next deterministic value, or zero if RNG is not armed.
    [[nodiscard]] std::uint64_t next_u64() noexcept {
        if (callback_ == nullptr) {
            return 0;
        }
        return callback_(state_);
    }
  private:
    Callback callback_ = nullptr;
    void* state_ = nullptr;
};

/// Wrapper around `NxdFrameContext` providing safe accessors.
class FrameContext {
  public:
    explicit FrameContext(const NxdFrameContext& ctx) noexcept : ctx_(ctx) {}

    [[nodiscard]] std::uint64_t frame_no() const noexcept {
        return ctx_.frame_no;
    }
    [[nodiscard]] std::int64_t sim_time_ns() const noexcept {
        return ctx_.sim_time_ns;
    }
    [[nodiscard]] std::int64_t hw_time_ns() const noexcept {
        return ctx_.hw_time_ns;
    }
    [[nodiscard]] std::uint32_t seed_version() const noexcept {
        return ctx_.seed_version;
    }

    /// Returns a deterministic RNG handle for this frame.
    [[nodiscard]] Rng rng() const noexcept {
        return Rng(ctx_);
    }

    /// Returns the raw ABI context.  The pointer is valid only for the
    /// duration of the callback.
    [[nodiscard]] const NxdFrameContext& raw() const noexcept {
        return ctx_;
    }
  private:
    const NxdFrameContext& ctx_;
};

/// Typed view of an input port buffer.
///
/// `Port` carries no allocation and is valid only for the duration of the
/// callback that produced it.
template <typename T>
class Port {
    static_assert(std::is_trivially_copyable_v<T>, "Port element must be trivially copyable");
  public:
    constexpr Port() noexcept = default;

    /// Creates a port view from a raw pointer and element count.
    /// `ptr` must point to `count` valid, aligned elements of `T` that
    /// outlive this view.
    Port(const T* ptr, std::size_t count) noexcept : span_(ptr, count) {}

    /// Creates a port view from an ABI port slot.
    explicit Port(const NxdPortSlot& slot) noexcept
        : span_(static_cast<const T*>(slot.data), slot.count) {}

    [[nodiscard]] std::span<const T> as_span() const noexcept {
        return span_;
    }
    [[nodiscard]] bool empty() const noexcept {
        return span_.empty();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return span_.size();
    }
  private:
    std::span<const T> span_{};
};

/// Writable typed output loan.
///
/// The component fills the loan during the extended step callback and
/// returns it; the loan references host-owned memory and must not outlive
/// the callback.
template <typename T>
class OutputLoan {
    static_assert(std::is_trivially_copyable_v<T>,
                  "OutputLoan element must be trivially copyable");
  public:
    constexpr OutputLoan() noexcept = default;

    /// Creates an output loan from a raw pointer and element count.
    /// `ptr` must point to `count` valid, aligned, writable elements of
    /// `T` that outlive this loan.
    OutputLoan(T* ptr, std::size_t count) noexcept : span_(ptr, count) {}

    /// Creates an output loan from an ABI port slot.
    explicit OutputLoan(NxdPortSlot& slot) noexcept
        : span_(static_cast<T*>(slot.data), slot.count) {}

    [[nodiscard]] std::span<T> as_span() noexcept {
        return span_;
    }
    [[nodiscard]] std::span<const T> as_span() const noexcept {
        return span_;
    }
    [[nodiscard]] bool empty() const noexcept {
        return span_.empty();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return span_.size();
    }
  private:
    std::span<T> span_{};
};

} // namespace nxd

#endif // NXD_SDK_HPP
