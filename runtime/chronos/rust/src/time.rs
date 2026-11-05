//! Integer-nanosecond Chronos time types.
//!
//! These types mirror the C++ declarations in
//! `runtime/chronos/include/nxd/runtime/chronos/time.hpp` and use `#[repr(C)]`
//! so they can cross the FFI boundary unchanged.

use core::ops::{Add, Neg, Sub};

/// Logical simulation time.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct SimTime {
    pub ns: i64,
}

/// Host monotonic clock observation.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct WallTime {
    pub ns: i64,
}

/// Common hardware/FPGA time base when running HIL.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct HwFpgaTime {
    pub ns: i64,
}

/// Signed elapsed time between two instants.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct Duration {
    pub ns: i64,
}

impl SimTime {
    /// Constructs a `SimTime` from a raw nanosecond count.
    #[must_use]
    pub const fn from_ns(ns: i64) -> Self {
        Self { ns }
    }

    /// Returns the underlying nanosecond count.
    #[must_use]
    pub const fn as_ns(self) -> i64 {
        self.ns
    }
}

impl WallTime {
    #[must_use]
    pub const fn from_ns(ns: i64) -> Self {
        Self { ns }
    }

    #[must_use]
    pub const fn as_ns(self) -> i64 {
        self.ns
    }
}

impl HwFpgaTime {
    #[must_use]
    pub const fn from_ns(ns: i64) -> Self {
        Self { ns }
    }

    #[must_use]
    pub const fn as_ns(self) -> i64 {
        self.ns
    }
}

impl Duration {
    #[must_use]
    pub const fn from_ns(ns: i64) -> Self {
        Self { ns }
    }

    #[must_use]
    pub const fn as_ns(self) -> i64 {
        self.ns
    }

    #[must_use]
    pub const fn from_micros(us: i64) -> Self {
        Self { ns: us * 1_000 }
    }

    #[must_use]
    pub const fn from_millis(ms: i64) -> Self {
        Self { ns: ms * 1_000_000 }
    }

    #[must_use]
    pub const fn from_seconds(s: i64) -> Self {
        Self {
            ns: s * 1_000_000_000,
        }
    }
}

impl Sub for SimTime {
    type Output = Duration;
    fn sub(self, rhs: Self) -> Duration {
        Duration::from_ns(self.ns - rhs.ns)
    }
}

impl Add<Duration> for SimTime {
    type Output = Self;
    fn add(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns + rhs.ns)
    }
}

impl Sub<Duration> for SimTime {
    type Output = Self;
    fn sub(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns - rhs.ns)
    }
}

impl Sub for WallTime {
    type Output = Duration;
    fn sub(self, rhs: Self) -> Duration {
        Duration::from_ns(self.ns - rhs.ns)
    }
}

impl Add<Duration> for WallTime {
    type Output = Self;
    fn add(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns + rhs.ns)
    }
}

impl Sub<Duration> for WallTime {
    type Output = Self;
    fn sub(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns - rhs.ns)
    }
}

impl Sub for HwFpgaTime {
    type Output = Duration;
    fn sub(self, rhs: Self) -> Duration {
        Duration::from_ns(self.ns - rhs.ns)
    }
}

impl Add<Duration> for HwFpgaTime {
    type Output = Self;
    fn add(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns + rhs.ns)
    }
}

impl Sub<Duration> for HwFpgaTime {
    type Output = Self;
    fn sub(self, rhs: Duration) -> Self {
        Self::from_ns(self.ns - rhs.ns)
    }
}

impl Add for Duration {
    type Output = Self;
    fn add(self, rhs: Self) -> Self {
        Self::from_ns(self.ns + rhs.ns)
    }
}

impl Sub for Duration {
    type Output = Self;
    fn sub(self, rhs: Self) -> Self {
        Self::from_ns(self.ns - rhs.ns)
    }
}

impl Neg for Duration {
    type Output = Self;
    fn neg(self) -> Self {
        Self::from_ns(-self.ns)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::{align_of, size_of};

    #[test]
    fn time_types_are_c_abi_sized() {
        assert_eq!(size_of::<SimTime>(), 8);
        assert_eq!(align_of::<SimTime>(), 8);
        assert_eq!(size_of::<WallTime>(), 8);
        assert_eq!(align_of::<WallTime>(), 8);
        assert_eq!(size_of::<HwFpgaTime>(), 8);
        assert_eq!(align_of::<HwFpgaTime>(), 8);
        assert_eq!(size_of::<Duration>(), 8);
        assert_eq!(align_of::<Duration>(), 8);
    }

    #[test]
    fn duration_conversion_constants_are_exact() {
        assert_eq!(Duration::from_micros(1).as_ns(), 1_000);
        assert_eq!(Duration::from_millis(1).as_ns(), 1_000_000);
        assert_eq!(Duration::from_seconds(1).as_ns(), 1_000_000_000);
    }

    #[test]
    fn sim_time_arithmetic_is_exact() {
        let t0 = SimTime::from_ns(0);
        let t1 = SimTime::from_ns(1_000_000);
        let dt = t1 - t0;
        assert_eq!(dt.as_ns(), 1_000_000);
        assert_eq!((t0 + dt).as_ns(), 1_000_000);
        assert_eq!((t1 - dt).as_ns(), 0);
    }

    #[test]
    fn duration_negation_is_exact() {
        let d = Duration::from_ns(500);
        assert_eq!((-d).as_ns(), -500);
    }
}
