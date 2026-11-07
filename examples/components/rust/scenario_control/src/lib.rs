//! Rust `scenario_control` plugin for ABI-1.
//!
//! Owns the `scenario/state` Blackboard entry, applies allowlisted commands at
//! the configured frame, and proves cross-language shared state between Rust
//! and C++ components.

#![no_std]

use core::ffi::{c_char, c_void};
use core::slice;
use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdComponentState, NxdFrameContext,
    NxdPortIoBundle, NxdStatus, NXD_ERROR_INVALID_ARGUMENT, NXD_OK,
};
use nxd_sdk::{status_from, Component, FrameContext};

mod gen {
    pub mod scenario_state {
        include!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/src/generated/ScenarioState.rs"
        ));
    }
    pub mod scenario_command {
        include!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/src/generated/ScenarioCommand.rs"
        ));
    }
}

pub use gen::scenario_command::ScenarioCommand;
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

const COMMANDS_PORT_HASH: u64 = fnv1a_hash(b"commands");

const MAX_PENDING: usize = 8;

#[repr(C)]
struct ScenarioControlState {
    scenario_entry: *mut NxdBlackboardEntry,
    pending: [ScenarioCommand; MAX_PENDING],
    pending_count: usize,
    current_value: u32,
    generation: u32,
}

impl ScenarioControlState {
    const fn new() -> Self {
        Self {
            scenario_entry: core::ptr::null_mut(),
            pending: [ScenarioCommand {
                command_id: 0,
                _pad0: [0; 4],
                apply_frame_no: 0,
                value: 0,
                _pad1: [0; 4],
            }; MAX_PENDING],
            pending_count: 0,
            current_value: 0,
            generation: 0,
        }
    }

    fn apply_command(&mut self, cmd: ScenarioCommand, frame_no: u64) {
        if cmd.apply_frame_no == frame_no {
            self.current_value = cmd.value;
            self.generation = self.generation.wrapping_add(1);
        } else if cmd.apply_frame_no > frame_no && self.pending_count < MAX_PENDING {
            self.pending[self.pending_count] = cmd;
            self.pending_count += 1;
        }
    }

    fn process_pending(&mut self, frame_no: u64) {
        let mut i = 0;
        while i < self.pending_count {
            if self.pending[i].apply_frame_no == frame_no {
                self.current_value = self.pending[i].value;
                self.generation = self.generation.wrapping_add(1);
                self.pending_count -= 1;
                self.pending[i] = self.pending[self.pending_count];
            } else {
                i += 1;
            }
        }
    }

    fn write_state(&mut self, frame_no: u64) -> NxdStatus {
        if self.scenario_entry.is_null() {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        let state = ScenarioState {
            value: self.current_value,
            generation: self.generation,
        };
        let rc = unsafe {
            nxd_blackboard_write(
                self.scenario_entry,
                &state as *const ScenarioState as *const c_void,
                core::mem::size_of::<ScenarioState>() as u64,
                frame_no,
            )
        };
        if rc < 0 {
            return NXD_ERROR_INVALID_ARGUMENT;
        }
        NXD_OK
    }
}

impl Component for ScenarioControlState {
    fn create() -> Result<Self, NxdStatus> {
        Ok(Self::new())
    }

    fn initialize(&mut self, config: &str) -> Result<(), NxdStatus> {
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
        parse_config(config, &mut self.pending, &mut self.pending_count);
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
    state: *mut NxdComponentState,
    ctx: *const NxdFrameContext,
    inputs: *const NxdPortIoBundle,
    _outputs: *mut NxdPortIoBundle,
) -> NxdStatus {
    if state.is_null() || ctx.is_null() {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    let Some(s) = nxd_sdk::state_from_ptr::<ScenarioControlState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_no = ctx_ref.frame_no;

    if !inputs.is_null() {
        let bundle = &*inputs;
        if !bundle.slots.is_null() {
            let slots = slice::from_raw_parts(bundle.slots, bundle.count);
            for slot in slots {
                if slot.port_name_hash == COMMANDS_PORT_HASH
                    && slot.count > 0
                    && !slot.data.is_null()
                {
                    let cmd = *(slot.data as *const ScenarioCommand);
                    (*s).apply_command(cmd, frame_no);
                }
            }
        }
    }

    (*s).process_pending(frame_no);
    (*s).write_state(frame_no)
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
    static mut STATE: ScenarioControlState = ScenarioControlState::new();
    STATE = ScenarioControlState::new();
    &raw mut STATE as *mut NxdComponentState
}

unsafe extern "C" fn destroy(_state: *mut NxdComponentState) {}

unsafe extern "C" fn initialize(
    state: *mut NxdComponentState,
    config: *const c_char,
    config_len: usize,
) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<ScenarioControlState>(state) else {
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
    let Some(s) = nxd_sdk::state_from_ptr::<ScenarioControlState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = FrameContext::from_raw(ctx_ref);
    status_from((*s).arm(frame_ctx))
}

unsafe extern "C" fn step(state: *mut NxdComponentState, ctx: *const NxdFrameContext) -> NxdStatus {
    let Some(s) = nxd_sdk::state_from_ptr::<ScenarioControlState>(state) else {
        return NXD_ERROR_INVALID_ARGUMENT;
    };
    let ctx_ref = &*ctx;
    let frame_ctx = FrameContext::from_raw(ctx_ref);
    status_from((*s).step(frame_ctx))
}

// Blackboard C API used by this component.
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

const fn fnv1a_hash(bytes: &[u8]) -> u64 {
    let mut hash: u64 = 0xcbf29ce484222325;
    let mut i = 0;
    while i < bytes.len() {
        hash ^= bytes[i] as u64;
        hash = hash.wrapping_mul(0x100000001b3);
        i += 1;
    }
    hash
}

fn parse_config(config: &str, pending: &mut [ScenarioCommand], count: &mut usize) {
    *count = 0;
    let mut remaining = config.as_bytes();
    while !remaining.is_empty() && *count < pending.len() {
        let (frame, rest) = parse_u64(remaining);
        if rest.is_empty() || rest[0] != b':' {
            break;
        }
        let rest = &rest[1..];
        let (value, rest) = parse_u64(rest);
        pending[*count] = ScenarioCommand {
            command_id: 0,
            _pad0: [0; 4],
            apply_frame_no: frame,
            value: value as u32,
            _pad1: [0; 4],
        };
        *count += 1;
        if rest.is_empty() || rest[0] != b',' {
            break;
        }
        remaining = &rest[1..];
    }
}

fn parse_u64(input: &[u8]) -> (u64, &[u8]) {
    let mut value: u64 = 0;
    let mut i = 0;
    while i < input.len() && input[i].is_ascii_digit() {
        value = value
            .wrapping_mul(10)
            .wrapping_add((input[i] - b'0') as u64);
        i += 1;
    }
    (value, &input[i..])
}
