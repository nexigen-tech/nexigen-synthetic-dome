# Design

## Context

See `proposal.md` for motivation. The project already has a contract IDL (`tools/nxdc`), generated C++ and Rust bindings, an ABI-1 plugin table, a component host, and a headless test harness. The goal of this change is to formalize the two primitive contracts (`SignalValue`, `SampleHeader`) and to harden the requirements around stable names, semantic versioning, and cross-language verification that the existing main specs imply but do not state explicitly.

## Goals / Non-Goals

**Goals:**
- Add durable delta specs for `contracts/signal-value` and `contracts/sample-header`.
- Strengthen `contracts/schema`, `components/descriptor`, `components/host`, and `components/testing` with explicit requirements for stable names, semver, `nexigen::app::Infrastructure` integration, thread-spec application, and golden binary vectors.
- Ensure `SampleHeader` is treated as a first-class contract and, where practical, align the runtime transport user header with it.
- Verify that existing golden C++/Rust tests and allocation-after-arm tests satisfy the acceptance criteria.

**Non-Goals:**
- No new runtime features (Chronos, Composer, virtual sources, panels, recorder).
- No changes to CANopen, video codecs beyond the existing skeleton contracts, or HIL gateways.
- No new component language or ABI version.

## Decisions

### Keep the contract-first code-generation pipeline
- Contracts stay as YAML under `contracts/`. `nxdc` validates them and emits C++23 POD headers and Rust `#[repr(C)]` modules at build time.
- Rationale: this is the existing pipeline; it already enforces fixed layouts, `layout_hash`, and cross-language assertions.
- Alternative considered: hand-writing headers — rejected because it would lose the canonical `layout_hash` and cold-path descriptors.

### Treat `SampleHeader` as the canonical metadata contract
- `SampleHeader.yaml` already exists. This change makes it a first-class durable spec and aligns the iceoryx2 user header type with its layout (via static assertions or by reusing the generated struct).
- Rationale: every transported sample needs the same metadata; a single contract avoids drift between the data-plane header and the IDL.
- Trade-off: renaming or resizing the transport header is an ABI break for existing pub/sub services, so the change must keep the existing byte layout.

### Use the existing ABI-1 table and `cbindgen` for cross-language ABI
- The C ABI header is generated from `sdk/rust/nxd-abi` by `cbindgen`; C++ wraps it. Rust plugins use `nxd-sdk` safe wrappers.
- Rationale: this gives one source of truth for struct layouts and status codes and already supports both legacy `step` and port-aware `step_io` callbacks.

### Verify cross-language equivalence with golden fixtures
- Golden tests live in `tools/nxdc/tests/golden.rs` and `tests/host_port_io_golden_test.cpp`. They compile small C++ binaries from generated headers and compare bytes with Rust.
- Rationale: compile-time layout assertions are necessary but not sufficient; golden vectors catch representation mismatches in enum values, padding, and endianness.

### Detect allocation-after-arm with test-only allocators
- C++ uses a counting allocator injected into selected test plugins; Rust uses a test allocator that fails on allocation after arming.
- Rationale: production hot path must stay allocation-free, but we cannot instrument every production build.

### Host owns one `nexigen::app::Infrastructure` per process
- `nxd_host` initializes Infrastructure before the first plugin is loaded and stops all components before Infrastructure shutdown.
- Rationale: this matches `nexigen-cpp` ownership rules and ensures tracer/metrics flush after component work stops.

## Risks / Trade-offs

- **User header alignment risk**: changing the transport `SampleMetadata` struct to match `SampleHeader` could break existing iceoryx2 service compatibility. Mitigation: keep size and field layout identical; use `static_assert` to prove equivalence, and version the service payload name separately.
- **Spec overlap risk**: some added requirements echo existing main-spec requirements. Mitigation: use distinct requirement names and scenarios so validation treats them as additive.
- **Existing implementation drift**: the codebase already implements many of these capabilities; tasks must verify rather than rewrite. Mitigation: tasks start with a gap audit against the delta specs before adding any missing code.

## Migration Plan

1. Create the delta specs and design in `openspec/changes/add-contracts-and-component-sdk/`.
2. During apply, audit existing code against the delta specs and add only missing tests or static assertions (for example, `SampleHeader` ↔ transport user-header layout equivalence).
3. Run Debug, Release, ASan, and Rust test matrices.
4. Archive the change after `ctest` and `cargo test` pass and the spec delta requirements are verified.
