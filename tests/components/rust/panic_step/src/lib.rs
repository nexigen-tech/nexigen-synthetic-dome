//! Test plugin that panics during `step`.
//!
//! The component is named `PanicRust`.  When `step` is called it invokes
//! `panic!("injected step panic")`.  The workspace profile uses
//! `panic = "abort"`, so the panic aborts the process.

#![no_std]

use core::ffi::c_char;
use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdComponentState, NxdFrameContext, NxdStatus,
    NXD_OK,
};
use nxd_sdk::{status_from, Component, FrameContext};

static mut STATE: PanicState = PanicState::new();

#[repr(C)]
struct PanicState;

impl PanicState {
    const fn new() -> Self {
        Self
    }
}

impl Component for PanicState {
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
        panic!("injected step panic");
    }
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe { libc::abort() }
}

/// ABI-1 entry symbol.
///
/// # Safety
///
/// The returned pointer must remain valid for the lifetime of the shared
/// library mapping.  This function is `unsafe` only because it returns a raw
/// pointer; the underlying table is immutable after initialization.
#[no_mangle]
pub unsafe extern "C" fn nxd_abi_entry() -> *const NxdAbiTable {
    static mut TABLE: NxdAbiTable = unsafe {
        nxd_sdk::build_abi_table(
            [0; 64],
            NxdAbiVersion {
                major: 1,
                minor: 0,
                patch: 0,
            },
            NxdAbiFingerprint {
                value: [
                    0x01, 0x13, 0xf6, 0x17, 0xb2, 0xa2, 0x1f, 0x87, 0x92, 0xd7, 0x7a, 0xfa, 0x9f,
                    0xea, 0x3c, 0x84, 0xee, 0x4c, 0x35, 0x90, 0x9c, 0x7b, 0xec, 0x57, 0xa3, 0xd8,
                    0x2a, 0xa4, 0xf2, 0x1b, 0x45, 0x99,
                ],
            },
            create,
            destroy,
            initialize,
            arm,
            step,
            stop,
        )
    };

    let name = b"DeterministicCounter\0";
    for (i, b) in name.iter().enumerate() {
        TABLE.component_name[i] = *b as c_char;
    }

    &raw const TABLE
}

unsafe extern "C" fn create() -> *mut NxdComponentState {
    STATE = PanicState::new();
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
    let Some(s) = nxd_sdk::state_from_ptr::<PanicState>(state) else {
        return nxd_abi::NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).arm(frame_ctx))
}

unsafe extern "C" fn step(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<PanicState>(state) else {
        return nxd_abi::NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).step(frame_ctx))
}

unsafe extern "C" fn stop(_state: *mut NxdComponentState) {}
