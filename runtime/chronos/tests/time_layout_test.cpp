/* Cross-language layout and conversion invariants for Chronos time types.
 *
 * Compares the C++ definitions against the cbindgen-generated C header produced
 * from the Rust crate so that size, alignment, and field offsets match.
 */

#include "nxd/runtime/chronos/time.hpp"

extern "C" {
#include "nxd_runtime_chronos.h"
}

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace chronos = nxd::runtime::chronos;

// C++ layout guarantees.
static_assert(sizeof(chronos::SimTime) == sizeof(std::int64_t));
static_assert(alignof(chronos::SimTime) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<chronos::SimTime>);
static_assert(std::is_trivially_copyable_v<chronos::SimTime>);

static_assert(sizeof(chronos::WallTime) == sizeof(std::int64_t));
static_assert(alignof(chronos::WallTime) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<chronos::WallTime>);
static_assert(std::is_trivially_copyable_v<chronos::WallTime>);

static_assert(sizeof(chronos::HwFpgaTime) == sizeof(std::int64_t));
static_assert(alignof(chronos::HwFpgaTime) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<chronos::HwFpgaTime>);
static_assert(std::is_trivially_copyable_v<chronos::HwFpgaTime>);

static_assert(sizeof(chronos::Duration) == sizeof(std::int64_t));
static_assert(alignof(chronos::Duration) == alignof(std::int64_t));
static_assert(std::is_standard_layout_v<chronos::Duration>);
static_assert(std::is_trivially_copyable_v<chronos::Duration>);

// Cross-language equivalence (C++ type vs. cbindgen-generated C struct).
static_assert(sizeof(chronos::SimTime) == sizeof(SimTime));
static_assert(alignof(chronos::SimTime) == alignof(SimTime));
static_assert(offsetof(chronos::SimTime, ns) == offsetof(SimTime, ns));

static_assert(sizeof(chronos::WallTime) == sizeof(WallTime));
static_assert(alignof(chronos::WallTime) == alignof(WallTime));
static_assert(offsetof(chronos::WallTime, ns) == offsetof(WallTime, ns));

static_assert(sizeof(chronos::HwFpgaTime) == sizeof(HwFpgaTime));
static_assert(alignof(chronos::HwFpgaTime) == alignof(HwFpgaTime));
static_assert(offsetof(chronos::HwFpgaTime, ns) == offsetof(HwFpgaTime, ns));

static_assert(sizeof(chronos::Duration) == sizeof(Duration));
static_assert(alignof(chronos::Duration) == alignof(Duration));
static_assert(offsetof(chronos::Duration, ns) == offsetof(Duration, ns));

// Conversion invariants -------------------------------------------------------

static_assert((chronos::SimTime{1'000'000} - chronos::SimTime{0}).ns == 1'000'000);
static_assert(
    (chronos::SimTime{0} + chronos::duration_ns(1'000'000)).ns == 1'000'000);
static_assert(
    (chronos::SimTime{1'000'000} - chronos::duration_ns(1'000'000)).ns == 0);

static_assert(chronos::nanoseconds_per_microsecond == 1'000);
static_assert(chronos::nanoseconds_per_millisecond == 1'000'000);
static_assert(chronos::nanoseconds_per_second == 1'000'000'000);

int main() {
    return 0;
}
