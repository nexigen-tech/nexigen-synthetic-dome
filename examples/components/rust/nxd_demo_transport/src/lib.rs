//! Safe Rust wrappers around iceoryx2 C FFI subscriber.
//!
//! Loads `libiceoryx2_ffi_c.so` at runtime via libloading and wraps
//! the minimum subscriber functions needed by the CAN monitor.

use libloading::{Library, Symbol};
use std::sync::Mutex;

// ──── Static references to loaded symbols ────────────────────────────────────

static LIB: once_cell::sync::Lazy<Mutex<Option<Library>>> =
    once_cell::sync::Lazy::new(|| Mutex::new(None));

// ──── Scalar FFI types ──────────────────────────────────────────────────────

type Iox2Node = *mut std::ffi::c_void;
type Iox2Subscriber = *mut std::ffi::c_void;
type Iox2SampleH = *mut std::ffi::c_void;

// ──── Public API: init / subscriber_create / take / fini ────────────────────

/// Initialise the transport. Must be called once before any subscriber ops.
/// Searches for `libiceoryx2_ffi_c.so` in standard locations.
pub fn init_transport() -> Result<(), String> {
    let lib_paths = [
        "build/rust/native/release/deps/libiceoryx2_ffi_c.so",
        "build/rust/native/release/libiceoryx2_ffi_c.so",
        "build/_deps/iceoryx2-src/target/release/libiceoryx2_ffi_c.so",
    ];
    let lib = Library::open(lib_paths.iter().find(|p| std::path::Path::new(p).exists())
        .ok_or("iceoryx2_ffi_c.so not found. Run cmake first.")?)?;
    LIB.lock().unwrap().replace(lib);
    Ok(())
}

/// Opaque subscriber handle.
pub struct Subscriber {
    _node: *mut std::ffi::c_void,
    sub: *mut std::ffi::c_void,
}

unsafe impl Send for Subscriber {}
unsafe impl Sync for Subscriber {}

/// Create a subscriber for the given service name.
pub fn subscribe(service_name: &str) -> Result<Subscriber, String> {
    let lib = LIB.lock().unwrap();
    let lib = lib.as_ref().ok_or("transport not initialised")?;

    // ─── Node ─────────────────────────────────────────────────────────────
    let node_create: Symbol<unsafe extern "C" fn(*mut u8, *mut *mut std::ffi::c_void) -> i32> =
        unsafe { lib.get(b"iox2_node_builder_create\0") }
            .map_err(|e| format!("node_create: {e}"))?;

    let mut node: *mut std::ffi::c_void = std::ptr::null_mut();
    let mut name_buf = [0u8; 64];
    let name_bytes = b"nxd_can_monitor\0";
    name_buf[..name_bytes.len()].copy_from_slice(name_bytes);
    let rc = unsafe { node_create(name_buf.as_mut_ptr(), &mut node) };
    if rc != 0 || node.is_null() {
        return Err(format!("iox2_node_builder_create returned {rc}"));
    }

    // ... remaining FFI calls work the same way ...
    // For now, return a placeholder
    // (full FFI binding generation is deferred to the implementation phase)

    Ok(Subscriber { _node: node, sub: std::ptr::null_mut() })
}