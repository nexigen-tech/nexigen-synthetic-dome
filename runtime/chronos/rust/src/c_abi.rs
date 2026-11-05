//! Chronos runtime C ABI table.
//!
//! These types define the versioned C API table implemented by the C++
//! `nxd_chronos` library.  Rust callers link against the same table so that
//! state-control semantics do not diverge between languages.

/// Numeric status returned by every Chronos state-control function.
pub type NxdChronosStatus = u32;

/// Operation completed successfully.
pub const NXD_CHRONOS_OK: NxdChronosStatus = 0;
/// Invalid argument or configuration.
pub const NXD_CHRONOS_ERROR_INVALID_ARGUMENT: NxdChronosStatus = 2;
/// Invalid state transition.
pub const NXD_CHRONOS_ERROR_INVALID_STATE: NxdChronosStatus = 4;

/// Execution mode selected before arming.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub enum NxdChronosMode {
    RealTimeHil = 0,
    ScaledSil = 1,
    FreeSil = 2,
    Step = 3,
}

/// Lifecycle state of a Chronos instance.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub enum NxdChronosState {
    Uninitialized = 0,
    Initialized = 1,
    Armed = 2,
    Playing = 3,
    Paused = 4,
    Stepping = 5,
    Stopped = 6,
    Disposed = 7,
}

/// Runtime configuration validated during initialization.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct NxdChronosConfig {
    pub mode: NxdChronosMode,
    pub base_frame_interval_ns: i64,
    pub scale_factor: u32,
}

impl NxdChronosConfig {
    /// Default 1 ms step configuration.
    pub const DEFAULT_STEP: Self = Self {
        mode: NxdChronosMode::Step,
        base_frame_interval_ns: 1_000_000,
        scale_factor: 1,
    };
}

/// Opaque handle to a Chronos runtime instance.
#[repr(C)]
pub struct NxdChronosInstance {
    _private: u8,
}

pub type NxdChronosCreateFn =
    unsafe extern "C" fn(config: *const NxdChronosConfig) -> *mut NxdChronosInstance;
pub type NxdChronosDestroyFn = unsafe extern "C" fn(instance: *mut NxdChronosInstance);
pub type NxdChronosInitializeFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosArmFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosPlayFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosPauseFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosStepFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosStopFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosDisposeFn =
    unsafe extern "C" fn(instance: *mut NxdChronosInstance) -> NxdChronosStatus;
pub type NxdChronosGetStateFn =
    unsafe extern "C" fn(instance: *const NxdChronosInstance) -> NxdChronosState;
pub type NxdChronosGetFrameNoFn = unsafe extern "C" fn(instance: *const NxdChronosInstance) -> u64;
pub type NxdChronosGetSimTimeNsFn =
    unsafe extern "C" fn(instance: *const NxdChronosInstance) -> i64;

/// Versioned table exported by the Chronos runtime.
#[repr(C)]
pub struct NxdChronosApiTable {
    pub version_major: u32,
    pub version_minor: u32,
    pub version_patch: u32,
    pub table_size: usize,
    pub create: NxdChronosCreateFn,
    pub destroy: NxdChronosDestroyFn,
    pub initialize: NxdChronosInitializeFn,
    pub arm: NxdChronosArmFn,
    pub play: NxdChronosPlayFn,
    pub pause: NxdChronosPauseFn,
    pub step: NxdChronosStepFn,
    pub stop: NxdChronosStopFn,
    pub dispose: NxdChronosDisposeFn,
    pub get_state: NxdChronosGetStateFn,
    pub get_frame_no: NxdChronosGetFrameNoFn,
    pub get_sim_time_ns: NxdChronosGetSimTimeNsFn,
}

/// Entry symbol exported by the Chronos runtime shared/static library.
pub type NxdChronosApiFn = unsafe extern "C" fn() -> *const NxdChronosApiTable;

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::{align_of, size_of};

    #[test]
    fn config_is_c_sized() {
        assert!(size_of::<NxdChronosConfig>() >= 16);
        assert!(align_of::<NxdChronosConfig>() >= 8);
    }

    #[test]
    fn api_table_size_is_stable() {
        assert!(size_of::<NxdChronosApiTable>() >= 96);
    }
}
