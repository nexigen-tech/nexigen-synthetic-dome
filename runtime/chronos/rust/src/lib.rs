//! NXD Chronos runtime — Rust bindings.
//!
//! Provides deterministic simulation-time types and, eventually, bindings to
//! the versioned C state-control API exposed by `runtime/chronos`.

#![cfg_attr(not(test), no_std)]

pub mod c_abi;
pub mod time;

pub use time::{Duration, HwFpgaTime, SimTime, WallTime};

/// Returns the Chronos runtime ABI version implemented by this crate.
#[must_use]
pub const fn abi_version() -> u32 {
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_version_is_one() {
        assert_eq!(abi_version(), 1);
    }

    #[test]
    fn reexported_time_types_match_names() {
        let _ = SimTime::from_ns(0);
        let _ = WallTime::from_ns(0);
        let _ = HwFpgaTime::from_ns(0);
        let _ = Duration::from_ns(0);
    }
}
