/* NXD Chronos — integer-nanosecond time types.
 *
 * All Chronos time values are represented as signed 64-bit integers storing
 * whole nanoseconds.  The hot path avoids floating point; these types are
 * standard-layout, trivially copyable, and have C-compatible layout so the same
 * representation can be used from Rust.
 */

#ifndef NXD_RUNTIME_CHRONOS_TIME_HPP
#define NXD_RUNTIME_CHRONOS_TIME_HPP

#include <cstdint>
#include <type_traits>

namespace nxd::runtime::chronos {

/// Default base frame interval in nanoseconds (1 ms).
inline constexpr std::int64_t default_base_frame_interval_ns = 1'000'000;

inline constexpr std::int64_t nanoseconds_per_microsecond = 1'000;
inline constexpr std::int64_t nanoseconds_per_millisecond = 1'000'000;
inline constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;

/// Logical simulation time.
struct SimTime final {
    std::int64_t ns{0};

    [[nodiscard]] constexpr bool operator==(const SimTime&) const noexcept = default;
};

/// Host monotonic clock observation.
struct WallTime final {
    std::int64_t ns{0};

    [[nodiscard]] constexpr bool operator==(const WallTime&) const noexcept = default;
};

/// Common hardware/FPGA time base when running HIL.
struct HwFpgaTime final {
    std::int64_t ns{0};

    [[nodiscard]] constexpr bool operator==(const HwFpgaTime&) const noexcept = default;
};

/// Signed elapsed time between two instants.
struct Duration final {
    std::int64_t ns{0};

    [[nodiscard]] constexpr bool operator==(const Duration&) const noexcept = default;
};

// Construction helpers -------------------------------------------------------

[[nodiscard]] constexpr SimTime sim_time_ns(std::int64_t ns) noexcept {
    return SimTime{ns};
}

[[nodiscard]] constexpr WallTime wall_time_ns(std::int64_t ns) noexcept {
    return WallTime{ns};
}

[[nodiscard]] constexpr HwFpgaTime hw_fpga_time_ns(std::int64_t ns) noexcept {
    return HwFpgaTime{ns};
}

[[nodiscard]] constexpr Duration duration_ns(std::int64_t ns) noexcept {
    return Duration{ns};
}

// Exact arithmetic -----------------------------------------------------------

[[nodiscard]] constexpr Duration operator-(SimTime lhs, SimTime rhs) noexcept {
    return Duration{lhs.ns - rhs.ns};
}

[[nodiscard]] constexpr SimTime operator+(SimTime t, Duration d) noexcept {
    return SimTime{t.ns + d.ns};
}

[[nodiscard]] constexpr SimTime operator-(SimTime t, Duration d) noexcept {
    return SimTime{t.ns - d.ns};
}

[[nodiscard]] constexpr Duration operator-(WallTime lhs, WallTime rhs) noexcept {
    return Duration{lhs.ns - rhs.ns};
}

[[nodiscard]] constexpr WallTime operator+(WallTime t, Duration d) noexcept {
    return WallTime{t.ns + d.ns};
}

[[nodiscard]] constexpr WallTime operator-(WallTime t, Duration d) noexcept {
    return WallTime{t.ns - d.ns};
}

[[nodiscard]] constexpr Duration operator-(HwFpgaTime lhs, HwFpgaTime rhs) noexcept {
    return Duration{lhs.ns - rhs.ns};
}

[[nodiscard]] constexpr HwFpgaTime operator+(HwFpgaTime t, Duration d) noexcept {
    return HwFpgaTime{t.ns + d.ns};
}

[[nodiscard]] constexpr HwFpgaTime operator-(HwFpgaTime t, Duration d) noexcept {
    return HwFpgaTime{t.ns - d.ns};
}

[[nodiscard]] constexpr Duration operator+(Duration a, Duration b) noexcept {
    return Duration{a.ns + b.ns};
}

[[nodiscard]] constexpr Duration operator-(Duration a, Duration b) noexcept {
    return Duration{a.ns - b.ns};
}

[[nodiscard]] constexpr Duration operator-(Duration d) noexcept {
    return Duration{-d.ns};
}

// Compile-time layout guarantees ---------------------------------------------

static_assert(sizeof(SimTime) == sizeof(std::int64_t),
              "SimTime must be the same size as its underlying nanosecond value");
static_assert(alignof(SimTime) == alignof(std::int64_t),
              "SimTime must share the alignment of its underlying value");
static_assert(std::is_standard_layout_v<SimTime>,
              "SimTime must have standard layout for C/Rust interop");
static_assert(std::is_trivially_copyable_v<SimTime>,
              "SimTime must be trivially copyable");

static_assert(sizeof(WallTime) == sizeof(std::int64_t));
static_assert(alignof(WallTime) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<WallTime>);
static_assert(std::is_trivially_copyable_v<WallTime>);

static_assert(sizeof(HwFpgaTime) == sizeof(std::int64_t));
static_assert(alignof(HwFpgaTime) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<HwFpgaTime>);
static_assert(std::is_trivially_copyable_v<HwFpgaTime>);

static_assert(sizeof(Duration) == sizeof(std::int64_t));
static_assert(alignof(Duration) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<Duration>);
static_assert(std::is_trivially_copyable_v<Duration>);

} // namespace nxd::runtime::chronos

#endif // NXD_RUNTIME_CHRONOS_TIME_HPP
