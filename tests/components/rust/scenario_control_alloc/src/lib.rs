//! Allocation-after-arm test for the scenario_control blackboard path.
//!
//! Mirrors the hot path of `scenario_control`: writes `scenario/state` via the
//! Blackboard C API in `step_io`.  A counting global allocator makes the test
//! fail if any heap allocation happens after `arm`.

#![no_std]

extern crate alloc;

use alloc::alloc::Layout;
use core::ffi::{c_char, c_void};
use core::slice;
use core::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use libc::{c_void as libc_void, free, malloc, posix_memalign};
use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdComponentState, NxdFrameContext,
    NxdPortIoBundle, NxdStatus, NXD_ERROR_INVALID_ARGUMENT, NXD_ERROR_OUT_OF_MEMORY, NXD_OK,
};
use nxd_sdk::{status_from, Component, FrameContext};

mod gen {
    pub mod scenario_state {
        include!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/src/generated/ScenarioState.rs"
        ));
    }
}

pub use gen::scenario_state::ScenarioState;

const SCENARIO_STATE_NAME: &str = "scenario/state\0";

const SCENARIO_STATE_LAYOUT_HASH: [u8; 32] = [
    0x5b, 0x1f, 0xfb, 0xd5, 0x0f, 0x10, 0xca, 0x2b, 0xfe, 0x34, 0xb7, 0xb3, 0x29, 0x85, 0xa1, 0xeb,
    0xf0, 0x7f, 0x70, 0x5a, 0x20, 0x1a, 0x95, 0x3b, 0x1a, 0x5d, 0xca, 0x4c, 0xa6, 0x4b, 0x84, 0x35,
];

const DESCRIPTOR_HASH: [u8; 32] = [
    0x43, 0xf4, 0x37, 0x16, 0x8c, 0x2c, 0x1d, 0xc9, 0x19, 0x12, 0xc4, 0x29, 0x06, 0xb2, 0xe4, 0x6e,
    0x4d, 0x55, 0x91, 0x0d, 0xf8, 0x84, 0x3e, 0xd5, 0x61, 0xcd, 0xb8, 0x83, 0xf3, 0xc6, 0xa7, 0x50,
];

static ARMED: AtomicBool = AtomicBool::new(false);
static ALLOC_COUNT: AtomicUsize = AtomicUsize::new(0);

struct CountingAllocator;

unsafe impl alloc::alloc::GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if ARMED.load(Ordering::SeqCst) {
            ALLOC_COUNT.fetch_add(1, Ordering::SeqCst);
        }

        let size = layout.size().max(1);
        let align = layout.align();
        if align <= core::mem::align_of::<usize>() {
            malloc(size) as *mut u8
        } else {
            let mut ptr: *mut libc_void = core::ptr::null_mut();
            if posix_memalign(&mut ptr, align, size) != 0 {
                core::ptr::null_mut()
            } else {
                ptr as *mut u8
            }
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        if !ptr.is_null() {
            free(ptr as *mut libc_void);
        }
    }
}

#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

#[repr(C)]
struct AllocCheckState {
    scenario_entry: *mut NxdBlackboardEntry,
}

impl AllocCheckState {
    const fn new() -> Self {
        Self {
            scenario_entry: core::ptr::null_mut(),
        }
    }
}

impl Component for AllocCheckState {
    fn create() -> Result<Self, NxdStatus> {
        Ok(Self::new())
    }

    fn initialize(&mut self, _config: &str) -> Result<(), NxdStatus> {
        unsafe {
            nxd_blackboard_initialize();
        }
        self.scenario_entry = unsafe {
            nxd_blackboard_create_entry(
                SCENARIO_STATE_NAME.as_ptr() as *const c_char,
                core::mem::size_of::<ScenarioState>() as u64,
                SCENARIO_STATE_LAYOUT_HASH.as_ptr(),
            )
        };
        if self.scenario_entry.is_null() {
            return Err(NXD_ERROR_INVALID_ARGUMENT);
        }
        Ok(())
    }

    fn arm(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        ALLOC_COUNT.store(0, Ordering::SeqCst);
        ARMED.store(true, Ordering::SeqCst);
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
    state: *mut NxdComponentState,
    ctx: *const NxdFrameContext,
    _inputs: *const NxdPortIoBundle,
    _outputs: *mut NxdPortIoBundle,
) -> NxdStatus {
    if state.is_null() || ctx.is_null() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCheckState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };

    let value = (*ctx).frame_no as u32;
    let st = ScenarioState {
        value,
        generation: 1,
    };
    let rc = nxd_blackboard_write(
        (*s).scenario_entry,
        &st as *const ScenarioState as *const c_void,
        core::mem::size_of::<ScenarioState>() as u64,
        (*ctx).frame_no,
    );
    if rc < 0 {
        return NXD_ERROR_INVALID_ARGUMENT;
    }

    if ALLOC_COUNT.load(Ordering::SeqCst) > 0 {
        return NXD_ERROR_OUT_OF_MEMORY;
    }
    NXD_OK
}

unsafe extern "C" fn stop(_state: *mut NxdComponentState) {}

/// Returns the ABI-1 entry table for this component.
///
/// # Safety
///
/// The returned pointer must remain valid for the lifetime of the shared
/// library mapping.
#[no_mangle]
pub unsafe extern "C" fn nxd_abi_entry() -> *const NxdAbiTable {
    static mut TABLE: NxdAbiTable = unsafe {
        nxd_sdk::build_abi_table_with_step_io(
            [0; 64],
            NxdAbiVersion {
                major: 1,
                minor: 0,
                patch: 0,
            },
            NxdAbiFingerprint { value: [0; 32] },
            create,
            destroy,
            initialize,
            arm,
            step,
            stop,
            step_io,
        )
    };

    let name = b"ScenarioControl\0";
    for (i, b) in name.iter().enumerate() {
        TABLE.component_name[i] = *b as c_char;
    }
    TABLE.descriptor_hash.value = DESCRIPTOR_HASH;

    &raw const TABLE
}

unsafe extern "C" fn create() -> *mut NxdComponentState {
    static mut STATE: AllocCheckState = AllocCheckState::new();
    STATE = AllocCheckState::new();
    &raw mut STATE as *mut NxdComponentState
}

unsafe extern "C" fn destroy(_state: *mut NxdComponentState) {}

unsafe extern "C" fn initialize(
    state: *mut NxdComponentState,
    config: *const c_char,
    config_len: usize,
) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCheckState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let config_bytes = if config.is_null() || config_len == 0 {
        &[] as &[u8]
    } else {
        slice::from_raw_parts(config as *const u8, config_len)
    };
    let config_str = core::str::from_utf8(config_bytes).unwrap_or("");
    status_from((*s).initialize(config_str))
}

unsafe extern "C" fn arm(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCheckState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = FrameContext::from_raw(ctx_ref);
    status_from((*s).arm(frame_ctx))
}

unsafe extern "C" fn step(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<AllocCheckState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = FrameContext::from_raw(ctx_ref);
    status_from((*s).step(frame_ctx))
}

#[link(name = "nxd_blackboard")]
unsafe extern "C" {
    fn nxd_blackboard_initialize() -> i32;
    fn nxd_blackboard_create_entry(
        name: *const c_char,
        value_size: u64,
        layout_hash: *const u8,
    ) -> *mut NxdBlackboardEntry;
    fn nxd_blackboard_write(
        entry: *mut NxdBlackboardEntry,
        value: *const c_void,
        value_size: u64,
        apply_frame_no: u64,
    ) -> i64;
}

#[allow(non_camel_case_types)]
type NxdBlackboardEntry = c_void;
