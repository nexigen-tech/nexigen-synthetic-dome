# Tasks

## 1. Build foundation and canonical schema model

- [x] 1.1 Create the root CMake and pinned Cargo workspaces, connect the local nexigen package, and verify that empty C++23 and Rust targets configure and build with GCC and Cargo without warnings treated as errors being bypassed.
- [x] 1.2 Implement the contract/component YAML grammar and validated intermediate model in `nxdc`, and verify valid fixtures load while unbounded fields, malformed types, duplicate names, and invalid versions fail with field-specific diagnostics.
- [x] 1.3 Implement canonical schema JSON and full SHA-256 `layout_hash` generation, and verify byte-identical output across repeated runs, changed source paths, and field-description edits, plus a changed hash after a layout or wire-semantic edit.
- [x] 1.4 Document the IDL vocabulary, canonicalization rules, and `nxdc` CLI in `docs/contracts.md`, and verify every documented example is covered by a parser or golden test.

## 2. C++ and Rust contract generation

- [x] 2.1 Implement C++ POD emission for records, fixed-width scalars, fixed arrays, nested values, typed aggregates, and explicit padding, and verify compile-time size/alignment/offset and trivial-layout assertions for all fixtures.
- [x] 2.2 Implement Rust `#[repr(C)]` emission with `ZeroCopySend`, stable explicit type names, integer-newtype enums, and layout assertions, and verify generated crates compile and satisfy the zero-copy type checks for all fixtures.
- [x] 2.3 Implement bounded buffer and concrete capacity instantiation for schema templates, and verify duplicate concrete identities, oversized values, and inconsistent capacity declarations fail before emission.
- [x] 2.4 Generate cold-path field descriptors, diagnostic text, and JSON support, and verify descriptors expose names/types/offsets/units/ranges/enum labels while generated hot-path values invoke no parser, formatter, or allocator.
- [x] 2.5 Add deterministic regeneration tests and verify a clean regeneration produces byte-identical C++, Rust, canonical JSON, descriptors, and hashes from the same schemas.

## 3. Common, CAN, and video contracts

- [x] 3.1 Define `SampleHeader` and `SignalValue<T>` schemas and verify frame/source/sequence metadata, independent per-field quality/origin, zero-initialized padding, and matching C++/Rust golden bytes.
- [x] 3.2 Define bounded Classical/FD/XL CAN frame and batch instantiations and verify 8/64/2048-byte storage, raw DLC versus payload length, direction, protocol metadata, capacity errors, and empty-batch watermark behavior.
- [x] 3.3 Add CAN heartbeat and aggregate reception errors and verify `SelfRx`, hardware timestamps, CRC-error preservation, activity filtering, and computation of `[D, H)` independently of host delivery time.
- [x] 3.4 Define video stream/frame schemas for Raw, JPEG, H.264, H.265, AV1, H.266, `DeviceRaw`, and extension identifiers, and verify descriptor revisions, rational FPS, plane metadata, bounded payload, and unsupported-codec handling.
- [x] 3.5 Add video heartbeat and aggregate error flags and verify DeviceRaw opaque transport, no per-line regions, independent transport/content sequences, no heartbeat decode, and missing-lines/CRC status behavior.
- [x] 3.6 Add bidirectional C++/Rust golden integration tests for representative common, CAN, and video values and verify identical bytes, field values, type identities, major versions, and `layout_hash` values in both directions.

## 4. Component descriptors and plugin ABI

- [x] 4.1 Implement component descriptor compilation for identity, SDK version, ports, parameters, file inputs, thread roles, resources, determinism flags, and supported modes, and verify valid descriptors emit canonical JSON while contradictory or incomplete descriptors leave no partial output.
- [x] 4.2 Define the Rust ABI-1 crate and generate its C/C++ declaration with `cbindgen`, and verify table size, ABI fingerprint, SDK layout hash, entry signature, fixed status codes, and lifecycle function-pointer layouts match in C++ and Rust.
- [x] 4.3 Implement C++ and Rust SDK wrappers for component state, frame contexts, typed ports, deterministic RNG callbacks, and fixed telemetry handles, and verify equivalent calls produce identical frame data without exposing wall-clock, physical channels, CPUs, or endpoints.
- [x] 4.4 Document ABI ownership, buffer lifetimes, status handling, versioning, and panic rules in `docs/component-abi.md`, and compile the documented minimal C++ and Rust plugin examples against the actual generated ABI.

## 5. `nxd-host` lifecycle and infrastructure

- [x] 5.1 Implement the NXD Infrastructure policy and host startup/shutdown order, and verify logger, metrics, tracing, chrono, and signal lifecycle tests show Infrastructure initialized before plugin work and flushed after component stop.
- [x] 5.2 Implement shared-library loading and pre-initialization validation of ABI, target fingerprint, component identity, descriptor hash, contracts, and resources, and verify incompatible plugins are rejected before their initialize entry point.
- [x] 5.3 Implement paired create/destroy and initialize/arm/step/stop with reverse rollback, and verify every injected lifecycle failure prevents later stages, releases resources at most once, and preserves the original status.
- [x] 5.4 Integrate `core::ThreadSpec` validation and self-application for main/control and headless driver threads, and verify required failures prevent arming while best-effort partial reports remain visible.
- [x] 5.5 Implement the manual headless host driver and step-failure handling, and verify frame N failure records its numeric status/frame, prevents frame N+1, performs no hot-path formatting/allocation, and enters the normal stop path.

## 6. Headless harness and example components

- [x] 6.1 Implement caller-driven frame/input/output harnesses using the real ABI table and guarded buffers, and verify ordered frame delivery, output capture, deterministic seed control, and out-of-bounds detection under ASan.
- [x] 6.2 Add C++ test-only allocation-after-arm guards and verify an allocating C++ test component fails while the empty and deterministic non-allocating components pass without production instrumentation.
- [x] 6.3 Add the Rust counting `GlobalAlloc` test component and verify allocation after arming is detected, non-allocating code passes, and production component crates do not install the test allocator.
- [x] 6.4 Implement equivalent deterministic C++ and Rust example plugins and verify identical lifecycle transitions, RNG sequences, output bytes, and per-frame state hashes across repeated headless runs.
- [x] 6.5 Implement child-process Rust panic and C++ failure-injection tests, and verify abnormal termination or status prevents subsequent frames and never unwinds into the host or continues with partial component state.
- [x] 6.6 Document headless component authoring and test commands in `docs/component-sdk.md`, and verify the documented C++ and Rust examples run without iceoryx2, Chronos, runner, Qt, or Tauri.

## 7. Integration and verification

- [x] 7.1 Run the complete C++ Debug and Release suites and verify all correctness tests pass with C++23, `-fno-exceptions`, and `-fno-rtti`; record build/host details and any timing-only environment limitation separately.
- [x] 7.2 Run ThreadSanitizer and AddressSanitizer/UBSan suites and verify all tests pass with no race, lifetime, bounds, leak, or undefined-behavior findings.
- [x] 7.3 Build and test with Clang and run Rust `cargo fmt --check`, `cargo clippy -- -D warnings`, debug tests, and release tests, and verify every check succeeds.
- [x] 7.4 Run the empty-component ABI benchmark and allocation/memory report and verify it reports measured call overhead and bounded type sizes without claiming scheduler, transport, or end-to-end latency.
- [x] 7.5 Perform a clean-build regeneration and full cross-language integration run and verify no generated artifact, hash, or test result depends on build directory, crate/binary name, or stale generated files.
- [x] 7.6 Run `openspec validate add-contracts-and-component-sdk --strict` and verify every proposal capability has a corresponding spec, every requirement has a scenario, all tasks are complete, and the design has no unresolved scope-changing decision.
