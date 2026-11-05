# Proposal

## Why

Contracts, descriptors, and the component host lifecycle are already in place, but ABI-1 components cannot exchange typed data: the `step` callback only receives a frame context. Without port buffers and a zero-copy transport, no virtual source, monitor, or protocol model can actually pass samples. This change adds typed ports and a minimal transport so that components can publish and subscribe to logical sources deterministically.

## What Changes

- Extend ABI-1 so that `step` receives a read-only view of input port buffers and a writable view of output port buffers, all typed by contract layout hash.
- Extend `NxdAbiTable` with optional `step_io` callback while keeping the existing `step` callback for backward compatibility with components that have no ports.
- Extend the C++ and Rust SDK wrappers to expose `nxd::Port<T>` and `nxd::OutputLoan<T>` views that require no allocation on the hot path.
- Extend `host/` to allocate, bind, and validate port buffers per component instance, matching declared port contracts and checking `layout_hash` before arming.
- Add a minimal `runtime/transport` or `runtime/blackboard` module that wires logical source names to iceoryx2 publishers/subscribers for local multi-process graphs, or to shared-memory channels for headless tests.
- Update `runtime/composer` Dome Plan with explicit `Wire` entries connecting instance ports to logical source names and validating that every required port is bound.
- Update `runtime/let` to treat cross-partition port writes as LET boundaries requiring read-time and write-deadline fields.
- Update `components/testing` headless harness to supply typed port buffers from fixture data without launching processes or using iceoryx2.
- Add golden tests proving that a C++ component and a Rust component produce identical binary output for the same input port data.

**Out of scope:** distributed multi-node transport, external DMA buffers, QoS policies, dynamic port creation after arming, and full LET deadline enforcement beyond validation and recording.

## Capabilities

### New Capabilities

- `runtime/transport`: Zero-copy local transport for typed port samples; logical source-name binding; publisher/subscriber lifecycle; headless test-mode channel backend.
- `sdk/port-io`: Typed input/output port views in C++ and Rust; `layout_hash` validation; zero-allocation step I/O.
- `runtime/composer-plan-wiring`: Port wiring validation and explicit `Wire` entries in the Dome Plan; required-port and compatible-contract checks.
- `runtime/let-port-boundaries`: LET boundary declaration for cross-partition port wires; read-time and write-deadline metadata on samples.

### Modified Capabilities

- `components/host`: ABI `step` now receives typed input and output port buffers; the host validates port contracts, directions, and layout hashes during arming.
- `components/testing`: Headless harness supports typed input/output fixtures and deterministic port replay.

## Impact

- **New systems:** transport/blackboard backend, SDK port I/O wrappers, headless port fixtures.
- **Build:** new C++ and Rust targets for transport; regenerated ABI header; new host validation tests.
- **Dependencies:** iceoryx2 for local multi-process transport; existing `runtime/chronos` for frame/time context; existing contracts and descriptors.
- **Compatibility:** ABI-1 plugins without ports continue to work through the legacy `step` callback. New port-enabled plugins require the updated SDK and declare ports in their descriptors.
- **Hot-path rules:** port views are borrowed spans/loans; no allocation, copying, or locking occurs after arming. Transport loans are acquired before the step and published after.
- **Follow-on changes:** `add-virtual-can-skeleton`, `add-video-streaming-skeleton`, and `add-blackboard-rust-and-tauri-skeleton` build directly on typed port transport.