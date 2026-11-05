//! Rust port-I/O test plugin for cross-language golden verification.
//!
//! Implements the same logic as the C++ `nxd_test_port_io_cpp` plugin:
//! reads one `Sample` from the `in` port and writes `value + 1` to the
//! `out` port.

#![no_std]

use core::ffi::c_char;
use core::slice;
use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdComponentState, NxdFrameContext, NxdPortIoBundle,
    NxdStatus, NXD_ERROR_INVALID_ARGUMENT, NXD_OK,
};
use nxd_sdk::{status_from, Component, FrameContext};

static mut STATE: PortIoState = PortIoState::new();

#[repr(C)]
#[derive(Clone, Copy)]
struct Sample {
    sequence: u64,
    value: i64,
}

#[repr(C)]
struct PortIoState;

impl PortIoState {
    const fn new() -> Self {
        Self
    }
}

impl Component for PortIoState {
    fn create() -> Result<Self, NxdStatus> {
        Ok(Self::new())
    }

    fn initialize(&mut self, _config: &str) -> Result<(), NxdStatus> {
        Ok(())
    }

    fn arm(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        Ok(())
    }

    fn step(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        Ok(())
    }
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe { libc::abort() }
}

unsafe extern "C" fn step_io(
    _state: *mut NxdComponentState,
    _ctx: *const NxdFrameContext,
    inputs: *const NxdPortIoBundle,
    outputs: *mut NxdPortIoBundle,
) -> NxdStatus {
    if inputs.is_null() || outputs.is_null() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    let in_bundle = &*inputs;
    if in_bundle.count == 0 || in_bundle.slots.is_null() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    let in_slot = &*in_bundle.slots;
    let in_samples = slice::from_raw_parts(in_slot.data as *const Sample, in_slot.count);
    if in_samples.is_empty() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    let out_bundle = &mut *outputs;
    if out_bundle.count == 0 || out_bundle.slots.is_null() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    let out_slot = &mut *out_bundle.slots;
    let out_samples = slice::from_raw_parts_mut(out_slot.data as *mut Sample, out_slot.count);
    if out_samples.is_empty() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    out_samples[0].sequence = in_samples[0].sequence;
    out_samples[0].value = in_samples[0].value + 1;
    NXD_OK
}

unsafe extern "C" fn stop(_state: *mut NxdComponentState) {}

/// ABI-1 entry symbol.
///
/// # Safety
///
/// The returned pointer must remain valid for the lifetime of the shared
/// library mapping.  The underlying table is immutable after initialization.
#[no_mangle]
pub unsafe extern "C" fn nxd_abi_entry() -> *const NxdAbiTable {
    static mut TABLE: NxdAbiTable = unsafe {
        nxd_sdk::build_abi_table_with_step_io(
            [0; 64],
            NxdAbiVersion { major: 1, minor: 0, patch: 0 },
            NxdAbiFingerprint::ZERO,
            create,
            destroy,
            initialize,
            arm,
            step,
            stop,
            step_io,
        )
    };

    let name = b"PortIoTest\0";
    for (i, b) in name.iter().enumerate() {
        TABLE.component_name[i] = *b as c_char;
    }

    &raw const TABLE
}

unsafe extern "C" fn create() -> *mut NxdComponentState {
    STATE = PortIoState::new();
    &raw mut STATE as *mut NxdComponentState
}

unsafe extern "C" fn destroy(_state: *mut NxdComponentState) {}

unsafe extern "C" fn initialize(
    _state: *mut NxdComponentState,
    _config: *const c_char,
    _config_len: usize,
) -> NxdStatus {
    NXD_OK
}

unsafe extern "C" fn arm(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<PortIoState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).arm(frame_ctx))
}

unsafe extern "C" fn step(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<PortIoState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).step(frame_ctx))
}
