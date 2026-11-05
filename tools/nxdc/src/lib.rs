//! NXD schema compiler library.
//!
//! This crate parses contract schemas and component descriptors from YAML,
//! validates them into a deterministic intermediate model, and emits canonical
//! JSON, C++23 headers, Rust `#[repr(C)]` bindings, and field descriptors.

pub mod canonical;
pub mod diag;
pub mod gen;
pub mod model;
pub mod parse;
pub mod validate;

use crate::diag::Diagnostics;
use crate::model::{ComponentDescriptor, ContractSchema};
use std::fs;
use std::path::Path;

/// Loads and validates a contract schema template from a YAML file.
///
/// For templates that declare `params`, this returns the unresolved template.
/// Use [`load_concrete_contracts`] to obtain the concrete instantiations that
/// should be emitted.
pub fn load_contract(path: &Path) -> Result<ContractSchema, Diagnostics> {
    let text = fs::read_to_string(path).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push(path.to_string_lossy(), format!("failed to read file: {e}"));
        d
    })?;
    let schema = parse::load_contract(path, &text)?;
    // Run validation once to ensure the template is well-formed; this also
    // checks that concrete instantiations are valid.  The returned template
    // is unchanged.
    validate::validate_and_instantiate(&schema)?;
    Ok(schema)
}

/// Loads a contract schema and returns all concrete instantiations that should
/// be emitted.  Non-parametric schemas return a single element vector.
pub fn load_concrete_contracts(path: &Path) -> Result<Vec<ContractSchema>, Diagnostics> {
    let text = fs::read_to_string(path).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push(path.to_string_lossy(), format!("failed to read file: {e}"));
        d
    })?;
    let schema = parse::load_contract(path, &text)?;
    validate::validate_and_instantiate(&schema)
}

/// Loads and validates a component descriptor from a YAML file.
pub fn load_component(path: &Path) -> Result<ComponentDescriptor, Diagnostics> {
    let text = fs::read_to_string(path).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push(path.to_string_lossy(), format!("failed to read file: {e}"));
        d
    })?;
    let descriptor = parse::load_component(path, &text)?;
    validate::validate_component(&descriptor)?;
    Ok(descriptor)
}
