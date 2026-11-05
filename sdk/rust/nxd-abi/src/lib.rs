//! NXD plugin ABI-1 C types.
//!
//! This crate defines the versioned C ABI table that every NXD plugin shared
//! library exports.  It contains only `#[repr(C)]` structs, opaque enums,
//! function-pointer aliases, and numeric status codes so that the same layout
//! is visible from C, C++, and Rust.
//!
//! ABI-1 plugins are built with `panic = "abort"`.  No Rust panic unwinds
//! across the C boundary.

#![no_std]

use core::ffi::{c_char, c_void};

/// Numeric status code returned by every lifecycle function.
pub type NxdStatus = u32;

/// Operation completed successfully.
pub const NXD_OK: NxdStatus = 0;
/// Unspecified error.
pub const NXD_ERROR_GENERAL: NxdStatus = 1;
/// Invalid argument passed to a lifecycle function.
pub const NXD_ERROR_INVALID_ARGUMENT: NxdStatus = 2;
/// Operation or capability not supported by the plugin.
pub const NXD_ERROR_NOT_SUPPORTED: NxdStatus = 3;
/// Component was already initialized.
pub const NXD_ERROR_ALREADY_INITIALIZED: NxdStatus = 4;
/// Component was not initialized.
pub const NXD_ERROR_NOT_INITIALIZED: NxdStatus = 5;
/// Allocation failed (cold path only).
pub const NXD_ERROR_OUT_OF_MEMORY: NxdStatus = 6;
/// ABI, fingerprint, or descriptor mismatch detected by host or plugin.
pub const NXD_ERROR_ABI_MISMATCH: NxdStatus = 7;

/// Version of the plugin ABI table itself.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NxdAbiVersion {
    pub major: u32,
    pub minor: u32,
    pub patch: u32,
}

impl NxdAbiVersion {
    /// Current ABI table version.
    pub const ABI_1: Self = Self {
        major: 1,
        minor: 0,
        patch: 0,
    };
}

/// 32-byte ABI fingerprint used for target/layout validation.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct NxdAbiFingerprint {
    pub value: [u8; 32],
}

impl NxdAbiFingerprint {
    /// All-zero fingerprint; reserved for unset/invalid values.
    pub const ZERO: Self = Self { value: [0; 32] };

    /// Fingerprint for the ABI-1 little-endian x86-64 target.
    /// This value is intentionally arbitrary and versioned; changing the
    /// ABI table changes the fingerprint.
    pub const ABI_1_TARGET: Self = Self {
        value: [
            0x4e, 0x58, 0x44, 0x5f, 0x41, 0x42, 0x49, 0x5f, 0x31, 0x5f, 0x6c, 0x65, 0x36, 0x34,
            0x5f, 0x78, 0x38, 0x36, 0x5f, 0x76, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
        ],
    };
}

/// Frame context passed to `arm` and `step`.
///
/// The host owns this value and all buffers reachable through it.  It is
/// valid only for the duration of the callback.  The component may read the
/// scalar fields and invoke the `rng` callback but must not retain pointers
/// beyond the callback return.
#[repr(C)]
pub struct NxdFrameContext {
    pub frame_no: u64,
    pub sim_time_ns: i64,
    pub hw_time_ns: i64,
    pub seed_version: u32,
    /// Deterministic RNG callback.  May be null if the host did not arm RNG.
    pub rng: unsafe extern "C" fn(state: *mut c_void) -> u64,
    /// Opaque RNG state pointer passed as the first argument to `rng`.
    pub rng_state: *mut c_void,
}

/// Opaque telemetry handle owned by the host.
#[repr(C)]
pub struct NxdTelemetryHandle {
    _private: u8,
}

/// Opaque component state handle created by the plugin and owned by the host.
#[repr(C)]
pub struct NxdComponentState {
    _private: u8,
}

/// Creates the plugin's private state.  Returns null on failure.
pub type NxdCreateFn = unsafe extern "C" fn() -> *mut NxdComponentState;

/// Destroys state previously created by `create`.
pub type NxdDestroyFn = unsafe extern "C" fn(state: *mut NxdComponentState);

/// Initializes the component from a UTF-8 configuration blob.
pub type NxdInitializeFn =
    unsafe extern "C" fn(state: *mut NxdComponentState, config: *const c_char, config_len: usize)
        -> NxdStatus;

/// Arms the component before the first frame.
pub type NxdArmFn =
    unsafe extern "C" fn(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus;

/// Executes one frame.
pub type NxdStepFn =
    unsafe extern "C" fn(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus;

/// Stops the component and releases frame-only resources.
pub type NxdStopFn = unsafe extern "C" fn(state: *mut NxdComponentState);

/// ABI-1 plugin table.  Each shared library exports exactly one symbol that
/// returns a pointer to this table.
///
/// The table size field lets the host reject plugins built against an
/// incompatible table layout even when the major version matches.  All
/// function pointers must be non-null for a valid ABI-1 plugin; `extension`
/// must be null for ABI-1.
#[repr(C)]
pub struct NxdAbiTable {
    pub abi_version: NxdAbiVersion,
    pub table_size: usize,
    pub fingerprint: NxdAbiFingerprint,
    pub sdk_layout_hash: NxdAbiFingerprint,
    /// Null-terminated component name, truncated to 63 bytes plus NUL.
    pub component_name: [c_char; 64],
    pub component_version: NxdAbiVersion,
    pub descriptor_hash: NxdAbiFingerprint,
    pub create: NxdCreateFn,
    pub destroy: NxdDestroyFn,
    pub initialize: NxdInitializeFn,
    pub arm: NxdArmFn,
    pub step: NxdStepFn,
    pub stop: NxdStopFn,
    /// Must be null for ABI-1.
    pub extension: *mut c_void,
}

impl NxdAbiTable {
    /// Expected table size for ABI-1.  The host rejects plugins whose
    /// `table_size` differs from this value.
    pub const ABI_1_SIZE: usize = core::mem::size_of::<Self>();
}

/// Entry symbol exported by every ABI-1 plugin shared library.
///
/// The returned pointer must remain valid for the lifetime of the shared
/// library mapping.  The table itself is read-only from the host side.
pub type NxdAbiEntryFn = unsafe extern "C" fn() -> *const NxdAbiTable;

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::{align_of, size_of};

    #[test]
    fn status_codes_are_fixed() {
        assert_eq!(NXD_OK, 0);
        assert_eq!(NXD_ERROR_GENERAL, 1);
        assert_eq!(NXD_ERROR_ABI_MISMATCH, 7);
    }

    #[test]
    fn abi_version_size_and_alignment() {
        assert_eq!(size_of::<NxdAbiVersion>(), 12);
        assert_eq!(align_of::<NxdAbiVersion>(), 4);
    }

    #[test]
    fn fingerprint_size_and_alignment() {
        assert_eq!(size_of::<NxdAbiFingerprint>(), 32);
        assert_eq!(align_of::<NxdAbiFingerprint>(), 1);
    }

    #[test]
    fn frame_context_size_and_alignment() {
        assert_eq!(size_of::<NxdFrameContext>() % 8, 0);
        assert!(align_of::<NxdFrameContext>() >= 8);
    }

    #[test]
    fn abi_table_size_is_expected() {
        assert_eq!(size_of::<NxdAbiTable>(), NxdAbiTable::ABI_1_SIZE);
        // The table must be large enough to hold the function pointers and
        // metadata; this value is part of the ABI contract.
        assert!(size_of::<NxdAbiTable>() >= 256);
    }

    #[test]
    fn abi_table_alignment_matches_pointer_size() {
        assert_eq!(align_of::<NxdAbiTable>(), size_of::<*const c_void>());
    }

    #[test]
    fn function_pointers_are_c_abi() {
        assert_eq!(size_of::<NxdCreateFn>(), size_of::<*const c_void>());
        assert_eq!(size_of::<NxdStepFn>(), size_of::<*const c_void>());
    }
}
