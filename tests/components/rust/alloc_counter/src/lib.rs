//! Test plugin that detects heap allocations after arming.
//!
//! The component is named `AllocCounterRust`.  After `arm` sets the global
//! `ARMED` flag, every heap allocation increments `ALLOC_COUNT`.  `step`
//! intentionally performs one allocation and then returns
//! `NXD_ERROR_OUT_OF_MEMORY` because the counter is now non-zero.

#![no_std]

extern crate alloc;

use alloc::boxed::Box;
use core::ffi::c_char;
use core::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use libc::{c_void, free, malloc};
use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdComponentState, NxdFrameContext, NxdStatus,
    NXD_OK,
};
use nxd_sdk::{status_from, Component, FrameContext};

static mut STATE: AllocCounterState = AllocCounterState::new();
static INITIALIZED: AtomicUsize = AtomicUsize::new(0);
static ARMED: AtomicBool = AtomicBool::new(false);
static ALLOC_COUNT: AtomicUsize = AtomicUsize::new(0);

/// Global allocator that counts allocations made after `ARMED` is set.
struct CountingAllocator;

unsafe impl alloc::alloc::GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: alloc::alloc::Layout) -> *mut u8 {
        if ARMED.load(Ordering::SeqCst) {
            ALLOC_COUNT.fetch_add(1, Ordering::SeqCst);
        }

        let size = layout.size().max(1);
        let align = layout.align();

        if align <= core::mem::align_of::<usize>() {
            malloc(size) as *mut u8
        } else {
            let mut ptr: *mut c_void = core::ptr::null_mut();
            if libc::posix_memalign(&mut ptr, align, size) != 0 {
                core::ptr::null_mut()
            } else {
                ptr as *mut u8
            }
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: alloc::alloc::Layout) {
        if !ptr.is_null() {
            free(ptr as *mut c_void);
        }
    }
}

#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

#[repr(C)]
struct AllocCounterState;

impl AllocCounterState {
    const fn new() -> Self {
        Self
    }
}

impl Component for AllocCounterState {
    fn create() -> Result<Self, NxdStatus> {
        ALLOC_COUNT.store(0, Ordering::SeqCst);
        ARMED.store(false, Ordering::SeqCst);
        Ok(Self::new())
    }

    fn initialize(&mut self, _config: &str) -> Result<(), NxdStatus> {
        Ok(())
    }

    fn arm(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        ALLOC_COUNT.store(0, Ordering::SeqCst);
        ARMED.store(true, Ordering::SeqCst);
        Ok(())
    }

    fn step(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        let _boxed: Box<u64> = Box::new(0xABCD);
        if ALLOC_COUNT.load(Ordering::SeqCst) > 0 {
            Err(nxd_abi::NXD_ERROR_OUT_OF_MEMORY)
        } else {
            Ok(())
        }
    }
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

/// ABI-1 entry symbol.
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
                    0x01, 0x13, 0xf6, 0x17, 0xb2, 0xa2, 0x1f, 0x87, 0x92, 0xd7, 0x7a, 0xfa,
                    0x9f, 0xea, 0x3c, 0x84, 0xee, 0x4c, 0x35, 0x90, 0x9c, 0x7b, 0xec, 0x57,
                    0xa3, 0xd8, 0x2a, 0xa4, 0xf2, 0x1b, 0x45, 0x99,
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

    &raw const TABLE as *const NxdAbiTable
}

unsafe extern "C" fn create() -> *mut NxdComponentState {
    STATE = AllocCounterState::new();
    INITIALIZED.store(1, Ordering::SeqCst);
    &raw mut STATE as *mut NxdComponentState
}

unsafe extern "C" fn destroy(_state: *mut NxdComponentState) {
    INITIALIZED.store(0, Ordering::SeqCst);
}

unsafe extern "C" fn initialize(
    _state: *mut NxdComponentState,
    _config: *const c_char,
    _config_len: usize,
) -> NxdStatus {
    NXD_OK
}

unsafe extern "C" fn arm(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCounterState>(state) else {
        return nxd_abi::NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).arm(frame_ctx))
}

unsafe extern "C" fn step(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCounterState>(state) else {
        return nxd_abi::NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = unsafe { FrameContext::from_raw(ctx_ref) };
    status_from((*s).step(frame_ctx))
}

unsafe extern "C" fn stop(_state: *mut NxdComponentState) {}
