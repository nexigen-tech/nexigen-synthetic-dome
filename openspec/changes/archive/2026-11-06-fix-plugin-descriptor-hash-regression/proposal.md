# Proposal: Fix plugin descriptor hash regression

## Why

After the component descriptor format evolved (notably with the addition of the
`SignalValueU64` contract in the walking-skeleton work), several C++ and Rust
example/test plugins still embed the old hard-coded `descriptor_hash`. The host's
`load_plugin` now rejects them during hash validation, leaving four host tests
red and the skeleton in an unstable state.

## What Changes

- Update the hard-coded `descriptor_hash` in every plugin that currently uses
  the stale value to match the current generated `.descriptor.hash` files.
- Ensure the intentionally-invalid `bad_descriptor_hash` test plugin still uses
  a deliberately wrong hash so the negative test continues to pass.
- Verify that the full `ctest` suite in the default `build/` directory passes.

No spec-level behavior changes: the plugin-load contract and descriptor
validation rules already exist in durable specs.

## Capabilities

No new or modified capabilities. This is a pure implementation bug-fix that
aligns existing plugin code with the already-specified validation rules.

## Impact

- `examples/components/cpp/counter`
- `examples/components/rust/counter`
- `tests/components/cpp/lifecycle`
- `tests/components/cpp/allocating`
- `tests/components/rust/alloc_counter`
- `tests/components/rust/panic_step`
- `tests/plugins/bad_plugin.cpp` (negative case only)

No public API or contract changes.
