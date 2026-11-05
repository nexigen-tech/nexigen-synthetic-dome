//! Code generation for C++23 POD, Rust `#[repr(C)]` types, and field descriptors.
//!
//! All generation operates on concrete [`ContractSchema`] values produced by
//! [`crate::validate::validate_and_instantiate`].

pub mod cpp;
pub mod descriptor;
pub mod rust;
