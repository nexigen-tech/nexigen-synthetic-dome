# Proposal

## Why

The current `runtime/blackboard` only has a headless in-process backend.  That
is enough for unit tests and for components loaded into the same `nxd-host`
process, but it does not satisfy the durable `runtime/blackboard` spec, which
requires an iceoryx2 backend so that panel processes can share Blackboard state
with the simulation.  Until that backend exists, the `scenario-panel` Tauri
backend cannot publish `ScenarioCommand` events or subscribe to decimated
`scenario/state` snapshots from a separate OS process.  This change unblocks the
panel workstream without changing component or panel code.

## What Changes

- Add a new `runtime/blackboard-iceoryx2-backend` capability: a multi-process,
  zero-copy Blackboard backend built on the same iceoryx2 C bindings already
  used by `runtime/transport`.
- Keep the existing `runtime/blackboard` C API unchanged so that C++ and Rust
  components, tests, and the future panel code continue to link against the
  same symbols (`nxd_blackboard_initialize`, `nxd_blackboard_create_entry`,
  `nxd_blackboard_open_entry`, `nxd_blackboard_write`, `nxd_blackboard_read`,
  etc.).
- Introduce a backend selection mechanism (compile-time or environment-driven)
  so that tests continue to use the headless backend by default while
  multi-process runs can select the iceoryx2 backend.
- Implement iceoryx2-backed entry creation, single-writer writes, reader
  subscription, revision/`apply_frame_no`/`generation` propagation, and
  `BlackboardUpdate` event emission across process boundaries.
- Add fork-based integration tests that prove a writer process and a reader
  process exchange Blackboard values with the same layout-hash validation and
  revision semantics as the headless backend.
- Update CMake build wiring for the new backend and tests; keep sanitizer
  compatibility in mind (the existing TSan seqlock workaround remains in place
  until a follow-up change).

Out of scope:
- Wiring the Tauri panel backend (deferred to `connect-scenario-panel-to-blackboard`).
- Launching the panel from `nexigen-run` (deferred to `add-panel-launcher-to-nexigen-run`).
- Replacing the headless backend or changing Blackboard contracts.
- Full TSan-compatible seqlock rewrite (deferred to `make-blackboard-seqlock-tsan-compatible`).

## Capabilities

### New Capabilities
- `runtime/blackboard-iceoryx2-backend`: Multi-process zero-copy Blackboard
  backend over iceoryx2.  Covers entry discovery, cross-process write/read,
  revision and generation propagation, `BlackboardUpdate` events, layout-hash
  validation, and lifecycle (initialize/stop) across processes.

### Modified Capabilities
- None.  The durable `runtime/blackboard` spec already requires both headless
  and iceoryx2 backends; this change implements the missing backend without
  altering spec-level behavior.

## Impact

- `runtime/blackboard/`: new iceoryx2 backend source files and backend selection.
- `runtime/transport/`: minor reuse of iceoryx2 service/node patterns (no API change).
- `tests/`: new fork-based Blackboard iceoryx2 integration tests.
- `CMakeLists.txt`, `runtime/blackboard/CMakeLists.txt`: build targets and
  iceoryx2 link dependencies.
- `panels/scenario-panel/`: no direct changes, but the backend it needs becomes
  available.
- Hot-path rules preserved: no heap allocations, mutexes, exceptions, or RT
  configuration regressions after arming.
