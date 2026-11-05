/* NXD Chronos runtime — public C++ interface.
 *
 * Deterministic simulation-time service that owns SimTime, WallTime, and
 * HwFpgaTime as integer nanosecond values and distributes frame contexts to
 * partitions.  This header is valid under C++23 with -fno-exceptions and
 * -fno-rtti.
 */

#ifndef NXD_RUNTIME_CHRONOS_HPP
#define NXD_RUNTIME_CHRONOS_HPP

#include <cstdint>

namespace nxd::runtime::chronos {

/// Default base frame interval in nanoseconds (1 ms).
inline constexpr std::int64_t default_base_frame_interval_ns = 1'000'000;

/// Minimum representable fast-frame interval in nanoseconds (100 µs).
inline constexpr std::int64_t min_fast_frame_interval_ns = 100'000;

/// Returns the Chronos runtime ABI version implemented by this build.
[[nodiscard]] constexpr std::uint32_t abi_version() noexcept {
    return 1u;
}

/// Human-readable name for diagnostics; never null.
[[nodiscard]] const char* runtime_name() noexcept;

} // namespace nxd::runtime::chronos

#endif // NXD_RUNTIME_CHRONOS_HPP
