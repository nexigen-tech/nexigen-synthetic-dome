# Proposal

## Why

Before any runtime features (Chronos, Composer, virtual sources, panels) can be composed reliably, the project needs a durable foundation for cross-language data contracts and the component plugin boundary. This change establishes the contract IDL, generated C++/Rust POD types, component descriptors, the ABI-1 host boundary, and the headless test harness that all later changes depend on.

## What Changes

- Add dedicated durable specs for the two primitive contracts every later stream builds on:
  - `SignalValue<T>` — typed scalar sample with per-field `Quality` and `Origin`.
  - `SampleHeader` — common per-sample metadata (frame number, simulation time, hardware time, source identity, sequence, flags).
- Harden the contract schema requirements around stable type names and semantic versioning so contract consumers can reject mismatches without relying on file paths or source formatting.
- Verify the component descriptor declares contract versions and layout hashes alongside logical ports.
- Confirm the host initializes `nexigen::app::Infrastructure` and applies `core::ThreadSpec` before any plugin work begins.
- Provide headless harness tests that prove a simple C++ and a simple Rust component pass through the same lifecycle without heap allocations after `arm`.

## Capabilities

### New Capabilities

- `contracts/signal-value`: Typed scalar sample contract with `value`, `quality`, and `origin`.
- `contracts/sample-header`: Common fixed-size metadata carried with every transported sample.

### Modified Capabilities

- `contracts/schema`: add requirements for stable contract type names and semantic-version compatibility, and clarify that `SampleHeader` is the canonical transport metadata contract.
- `components/descriptor`: add requirement that port contract references include the contract version and `layout_hash` needed for compatibility checks.
- `components/host`: add requirement that the host owns one `nexigen::app::Infrastructure` per process and applies declared thread specifications through `core::ThreadSpec` before stepping.
- `components/testing`: add requirement for golden cross-language binary fixtures proving C++ and Rust produce identical contract bytes.

## Impact

- `contracts/*.yaml` and `tools/nxdc` — contract definitions and code generation.
- `sdk/cpp/include/nxd/abi.hpp`, `sdk/rust/nxd-abi`, `sdk/rust/nxd-sdk` — ABI table and safe wrappers.
- `host/` — plugin loading, descriptor validation, lifecycle orchestration, thread role application.
- `tests/` — ABI layout, plugin equivalence, allocation-after-arm, headless harness, port I/O golden tests.
- No Composer, Chronos, CANopen stack, virtual sources, Qt/Tauri, or recorder work in this change.
