//! Field-specific diagnostics for schema and descriptor validation.
//!
//! Validation is all-or-nothing: every problem found during a single pass is
//! collected so callers can report every offending path, not just the first.

use std::fmt;

/// One diagnostic with a dotted path into the source document.
#[derive(Debug, Clone, PartialEq)]
pub struct Diagnostic {
    /// Logical path, e.g. `contract.fields[2].type`.
    pub path: String,
    /// Human-readable message.
    pub message: String,
}

impl Diagnostic {
    pub fn new(path: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            path: path.into(),
            message: message.into(),
        }
    }
}

/// A collected set of diagnostics, used as an error type.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct Diagnostics {
    pub errors: Vec<Diagnostic>,
}

impl Diagnostics {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, path: impl Into<String>, message: impl Into<String>) {
        self.errors.push(Diagnostic::new(path, message));
    }

    pub fn is_empty(&self) -> bool {
        self.errors.is_empty()
    }

    pub fn merge(&mut self, other: Diagnostics) {
        self.errors.extend(other.errors);
    }
}

impl fmt::Display for Diagnostics {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "nxdc: {} diagnostic(s)", self.errors.len())?;
        for err in &self.errors {
            writeln!(f, "  at {}: {}", err.path, err.message)?;
        }
        Ok(())
    }
}

impl std::error::Error for Diagnostics {}
