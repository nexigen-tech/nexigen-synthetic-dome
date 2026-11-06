# Proposal

## Why

The project already has a cross-language component SDK, Chronos/Composer/Transport
runtimes, and skeleton virtual CAN and video sources. The next walking-skeleton
capability is slow shared state and operator control: a Rust component must be
able to publish deterministic scenario state that C++ components react to, while
an operator panel can send allowlisted commands without affecting the
deterministic execution of models.

This change introduces the Blackboard runtime, the `scenario_control` Rust
component, and the `scenario_panel` Tauri + Svelte panel. It keeps high-rate data
on the existing port/transport path and reserves the Blackboard for
low-frequency, versioned, single-writer shared state (`dome/info`, `source/*`,
`scenario/state`, `video*/config`).

## What Changes

- Add durable delta specs for:
  - `runtime/blackboard` — single-writer entries, revisions, versioned POD values,
    `apply_frame_no`, `BlackboardUpdate` events.
  - `components/scenario-control` — Rust `cdylib` component that owns
    `scenario/state` and applies allowlisted commands at configured frames.
  - `panels/scenario-panel` — separate-process Tauri + Svelte panel that sends
    allowlisted commands and receives only decimated snapshots.
- Add Blackboard contracts (`BlackboardValue`, `BlackboardUpdate`, `ScenarioState`,
  `ScenarioCommand`) to `contracts/` and regenerate C++/Rust bindings.
- Add a `runtime/blackboard` C++ module with a headless backend for in-process
  tests and an iceoryx2 backend interface for multi-process panels.
- Add `examples/components/rust/scenario_control` as an ABI-1 `cdylib` plugin
  using `nxd-sdk`, plus its `components/scenario_control/component.yaml`
  descriptor.
- Add a C++ test consumer component that reads `scenario/state` from the
  Blackboard and publishes a typed output, proving that C++ components react to
  Rust-written entries.
- Add C++ Blackboard layout tests and Rust allocation-after-arm checks for
  `scenario_control`.
- Add `panels/scenario-panel` source (Tauri + Svelte) with a Rust backend that
  publishes `ScenarioCommand` events and subscribes to decimated Blackboard
  snapshots. The panel is built separately from the main workspace because its
  native dependencies (GTK/WebKit) are not part of the default test matrix.
- Update `Cargo.toml`, `CMakeLists.txt`, and test registrations.

## Capabilities

### New Capabilities

- `runtime/blackboard`: Slow shared state service with single-writer entries,
  revisions, `apply_frame_no`, versioned POD values, and `BlackboardUpdate`
  events for recording/replay.
- `components/scenario-control`: Rust `cdylib` component that is the single
  writer of the `scenario/state` Blackboard entry and applies allowlisted
  commands at the configured frame.
- `panels/scenario-panel`: Tauri + Svelte operator panel process that sends
  allowlisted commands and receives decimated Blackboard snapshots; closing it
  does not affect simulation output.

### Modified Capabilities

- `runtime/composer`: add requirement that a Dome Plan may declare Blackboard
  seed entries and optional panel processes so that `nexigen-run` can wire the
  Blackboard and panels in later changes.

## Impact

- `contracts/`, `tools/nxdc/` — new Blackboard and scenario contracts and
  generated bindings.
- `runtime/blackboard/` — new runtime module.
- `components/scenario_control/`, `examples/components/rust/scenario_control/` —
  new component and descriptor.
- `tests/` — Blackboard layout tests, cross-language consumer test,
  allocation-after-arm tests for the Rust component.
- `panels/scenario-panel/` — new Tauri + Svelte application source.
- `Cargo.toml`, `CMakeLists.txt` — workspace/build integration.
- No changes to CANopen stack, video codecs, TimePage, or high-rate transport
  semantics. FullHD video is not transmitted through Tauri IPC.
