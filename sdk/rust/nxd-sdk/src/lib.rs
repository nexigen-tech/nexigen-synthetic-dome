//! NXD plugin SDK for Rust components.
//!
//! This crate provides safe wrappers around the ABI-1 C types defined in
//! `nxd-abi`.  It is `no_std` and does not allocate on the hot path.

#![no_std]

pub use nxd_abi::{
    NxdAbiFingerprint, NxdAbiTable, NxdAbiVersion, NxdArmFn, NxdComponentState, NxdCreateFn,
    NxdDestroyFn, NxdFrameContext, NxdInitializeFn, NxdPortIoBundle, NxdPortSlot, NxdStatus,
    NxdStepFn, NxdStepIoFn, NxdStopFn, NXD_ERROR_ABI_MISMATCH, NXD_ERROR_ALREADY_INITIALIZED,
    NXD_ERROR_GENERAL, NXD_ERROR_INVALID_ARGUMENT, NXD_ERROR_NOT_INITIALIZED,
    NXD_ERROR_NOT_SUPPORTED, NXD_ERROR_OUT_OF_MEMORY, NXD_OK,
};

use core::ffi::{c_char, c_void};
use core::marker::PhantomData;
use core::ptr::NonNull;
use core::slice;

/// Borrowed view of a typed port buffer.
///
/// `Port` carries no allocation and is valid only for the duration of the
/// callback that produced it.
pub struct Port<'a, T: Copy> {
    data: NonNull<T>,
    len: usize,
    _life: PhantomData<&'a [T]>,
}

impl<'a, T: Copy> Port<'a, T> {
    /// Creates a port view from a raw pointer and element count.
    ///
    /// # Safety
    ///
    /// `ptr` must point to `len` valid, aligned elements of `T` that outlive
    /// the returned view.
    pub unsafe fn from_raw_parts(ptr: *mut T, len: usize) -> Option<Self> {
        Some(Self {
            data: NonNull::new(ptr)?,
            len,
            _life: PhantomData,
        })
    }

    /// Returns the number of elements in the port buffer.
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns true if the port buffer contains no elements.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Returns the buffer as a shared slice.
    pub fn as_slice(&self) -> &[T] {
        // SAFETY: constructed from valid raw parts and lifetime is bounded.
        unsafe { slice::from_raw_parts(self.data.as_ptr(), self.len) }
    }

    /// Returns the buffer as a mutable slice.
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        // SAFETY: constructed from valid raw parts and lifetime is bounded.
        unsafe { slice::from_raw_parts_mut(self.data.as_ptr(), self.len) }
    }
}

/// Writable typed output loan.
///
/// The component fills the loan during `step_io` and returns it.  The loan
/// references host-owned memory and must not outlive the callback.
pub struct OutputLoan<'a, T: Copy> {
    data: NonNull<T>,
    len: usize,
    _life: PhantomData<&'a mut [T]>,
}

impl<'a, T: Copy> OutputLoan<'a, T> {
    /// Creates an output loan from a raw pointer and element count.
    ///
    /// # Safety
    ///
    /// `ptr` must point to `len` valid, aligned elements of `T` that outlive
    /// the loan and are writable by the component during the step.
    pub unsafe fn from_raw_parts(ptr: *mut T, len: usize) -> Option<Self> {
        Some(Self {
            data: NonNull::new(ptr)?,
            len,
            _life: PhantomData,
        })
    }

    /// Returns the number of elements in the loan.
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns true if the loan contains no elements.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Returns the loan as a mutable slice.
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        // SAFETY: constructed from valid raw parts and lifetime is bounded.
        unsafe { slice::from_raw_parts_mut(self.data.as_ptr(), self.len) }
    }
}

/// Deterministic RNG exposed through the frame context.
pub struct Rng {
    callback: Option<unsafe extern "C" fn(*mut c_void) -> u64>,
    state: *mut c_void,
}

impl Rng {
    /// Creates an RNG wrapper from a raw frame context.
    ///
    /// # Safety
    ///
    /// `ctx` must be valid for the lifetime of the wrapper.
    pub unsafe fn from_context(ctx: &NxdFrameContext) -> Self {
        Self {
            // In ABI-1 a null function pointer means RNG is not armed.
            callback: if ctx.rng as usize == 0 {
                None
            } else {
                Some(ctx.rng)
            },
            state: ctx.rng_state,
        }
    }

    /// Returns the next deterministic random value, or zero if RNG is not armed.
    pub fn next_u64(&mut self) -> u64 {
        match self.callback {
            Some(cb) =>
            // SAFETY: callback is guaranteed by the host to be valid when armed.
            unsafe { (cb)(self.state) },
            None => 0,
        }
    }
}

/// Wrapper around `NxdFrameContext` providing safe accessors.
pub struct FrameContext<'a> {
    inner: &'a NxdFrameContext,
}

impl<'a> FrameContext<'a> {
    /// # Safety
    ///
    /// `ctx` must be valid for the lifetime `'a`.
    pub unsafe fn from_raw(ctx: &'a NxdFrameContext) -> Self {
        Self { inner: ctx }
    }

    pub fn frame_no(&self) -> u64 {
        self.inner.frame_no
    }

    pub fn sim_time_ns(&self) -> i64 {
        self.inner.sim_time_ns
    }

    pub fn hw_time_ns(&self) -> i64 {
        self.inner.hw_time_ns
    }

    pub fn seed_version(&self) -> u32 {
        self.inner.seed_version
    }

    /// Returns a deterministic RNG handle.
    pub fn rng(&self) -> Rng {
        // SAFETY: inner reference is valid for the context lifetime.
        unsafe { Rng::from_context(self.inner) }
    }
}

/// Trait implemented by Rust components.
///
/// The lifecycle methods correspond to the ABI-1 function pointers.  The
/// `State` type is the component's private state and must be `Sized` so it can
/// be boxed by the generated entry points.
pub trait Component: Sized {
    /// Creates the component state.
    fn create() -> Result<Self, NxdStatus>;

    /// Initializes the component from a UTF-8 configuration blob.
    fn initialize(&mut self, _config: &str) -> Result<(), NxdStatus> {
        Ok(())
    }

    /// Arms the component before the first frame.
    fn arm(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        Ok(())
    }

    /// Executes one frame.
    fn step(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> {
        Ok(())
    }

    /// Stops the component.
    fn stop(&mut self) {}
}

/// Converts a Rust `Result` into an `NxdStatus`.
pub fn status_from<T>(result: Result<T, NxdStatus>) -> NxdStatus {
    match result {
        Ok(_) => NXD_OK,
        Err(s) => s,
    }
}

/// Copies a Rust string into a fixed-size C char buffer, NUL-terminated.
///
/// Truncates with a NUL terminator if `name` is longer than `buf.len() - 1`.
pub fn write_component_name(buf: &mut [c_char], name: &str) {
    let bytes = name.as_bytes();
    let limit = buf.len().saturating_sub(1);
    let len = bytes.len().min(limit);
    for (i, b) in bytes[..len].iter().enumerate() {
        buf[i] = *b as c_char;
    }
    buf[len] = 0;
}

/// Helper to build an ABI-1 table from a component name/version and lifecycle
/// function pointers.
///
/// # Safety
///
/// All function pointers must satisfy the ABI-1 contract.
#[allow(clippy::too_many_arguments)]
pub const unsafe fn build_abi_table(
    name: [c_char; 64],
    version: NxdAbiVersion,
    descriptor_hash: NxdAbiFingerprint,
    create: NxdCreateFn,
    destroy: NxdDestroyFn,
    initialize: NxdInitializeFn,
    arm: NxdArmFn,
    step: NxdStepFn,
    stop: NxdStopFn,
) -> NxdAbiTable {
    NxdAbiTable {
        abi_version: NxdAbiVersion::ABI_1,
        table_size: core::mem::size_of::<NxdAbiTable>(),
        fingerprint: NxdAbiFingerprint::ABI_1_TARGET,
        sdk_layout_hash: NxdAbiFingerprint::ZERO,
        component_name: name,
        component_version: version,
        descriptor_hash,
        create,
        destroy,
        initialize,
        arm,
        step,
        stop,
        step_io: None,
    }
}

/// Helper to build an ABI-1 table with the optional `step_io` callback.
///
/// # Safety
///
/// All function pointers must satisfy the ABI-1 contract.
#[allow(clippy::too_many_arguments)]
pub const unsafe fn build_abi_table_with_step_io(
    name: [c_char; 64],
    version: NxdAbiVersion,
    descriptor_hash: NxdAbiFingerprint,
    create: NxdCreateFn,
    destroy: NxdDestroyFn,
    initialize: NxdInitializeFn,
    arm: NxdArmFn,
    step: NxdStepFn,
    stop: NxdStopFn,
    step_io: NxdStepIoFn,
) -> NxdAbiTable {
    NxdAbiTable {
        abi_version: NxdAbiVersion::ABI_1,
        table_size: core::mem::size_of::<NxdAbiTable>(),
        fingerprint: NxdAbiFingerprint::ABI_1_TARGET,
        sdk_layout_hash: NxdAbiFingerprint::ZERO,
        component_name: name,
        component_version: version,
        descriptor_hash,
        create,
        destroy,
        initialize,
        arm,
        step,
        stop,
        step_io: Some(step_io),
    }
}

/// Type-pun helper used by generated entry points to recover the concrete
/// component state from an opaque pointer.
///
/// # Safety
///
/// `ptr` must have been produced by the corresponding `create` function.
pub unsafe fn state_from_ptr<T>(ptr: *mut NxdComponentState) -> Option<*mut T> {
    NonNull::new(ptr).map(|p| p.as_ptr() as *mut T)
}
