# Design

## Context

The project already has:

- A C++/Rust component SDK with ABI-1 plugins (`sdk/cpp/include/nxd/abi.hpp`,
  `sdk/rust/nxd-abi`, `sdk/rust/nxd-sdk`).
- A host that loads `cdylib` plugins, validates descriptors, and runs the
  `initialize/arm/step/stop` lifecycle.
- `runtime/chronos`, `runtime/composer`, and `runtime/transport` modules.
- Skeleton virtual CAN and video sources.

There is no Blackboard, no `scenario_control`, and no Tauri/Svelte panel. This
change adds the slow-shared-state and operator-control slice of the walking
skeleton without disturbing the existing high-rate data path.

## Goals / Non-Goals

**Goals:**

- Add durable delta specs for `runtime/blackboard`,
  `components/scenario-control`, and `panels/scenario-panel`.
- Provide a Blackboard runtime with a headless backend for in-process tests and
  an iceoryx2 backend interface for multi-process panels.
- Implement `scenario_control` as a Rust ABI-1 `cdylib` that writes
  `scenario/state` and applies allowlisted commands at configured frames.
- Provide a C++ test consumer component that reads `scenario/state` from the
  Blackboard and proves cross-language reaction.
- Add C++ Blackboard layout tests and Rust allocation-after-arm checks.
- Provide a Tauri + Svelte panel source that sends allowlisted commands and
  receives decimated snapshots.

**Non-Goals:**

- Full multi-process iceoryx2 Blackboard optimization beyond the skeleton
  interface.
- Command queues, TimePage distribution, or high-rate signal transport over the
  Blackboard.
- Recording/replay of `BlackboardUpdate` events (events are emitted; recording
  is a later change).
- FullHD video streaming through Tauri IPC.
- Composer-side panel orchestration beyond declaring that a Dome Plan may list
  panel processes and Blackboard seed entries.

## Decisions

### Keep Blackboard as a runtime service separate from ports

- Components access the Blackboard through a runtime service handle obtained
  during initialization, not through high-rate ports. Ports continue to carry
  samples; the Blackboard carries slow, versioned, single-writer state.
- Rationale: this preserves the existing port semantics and hot-path guarantees
  while giving components a deterministic shared-state channel.

### Pre-allocate Blackboard entry storage and use a sequence lock

- Each entry has a fixed maximum value size configured at creation. The writer
  and readers use a lightweight sequence lock (atomic revision counter) so the
  hot path uses only atomics and copies, never heap allocation or mutexes.
- Rationale: satisfies the "no allocation after Armed" requirement and keeps
  reads wait-free.
- Trade-off: entries are bounded; oversized values must be split across entries.

### Rust `scenario_control` consumes commands through a typed input port

- The component declares an input port on `ScenarioCommand`. The panel (or a
  test harness) publishes commands to that logical source. The component queues
  commands internally on the cold path and applies the command value at the
  command's `apply_frame_no`.
- Rationale: this reuses the existing transport/port infrastructure, makes the
  component testable without Tauri, and still models the panel-to-control flow.

### C++ consumer reads `scenario/state` through the Blackboard service

- A small C++ test component (`nxd_test_scenario_consumer`) reads the
  `scenario/state` entry during `step` and writes a `SignalValue<uint32>` to its
  output port. The test verifies that the output matches the value applied by
  the Rust component.
- Rationale: this is the minimal cross-language proof required by the
  acceptance criteria.

### Panel is a separate OS process with an iceoryx2 backend

- The Tauri Rust backend subscribes to Blackboard entries and publishes
  `ScenarioCommand` events using the same transport/blackboard backend used by
  the runtime. The Svelte frontend receives only decimated snapshots via Tauri
  events/commands.
- Rationale: panels must not run inside the deterministic simulation process,
  and closing a panel must not change model outputs.

### Panel build is optional in the default workspace

- The `panels/scenario-panel/src-tauri` crate is not a member of the root
  `Cargo.toml` workspace because Tauri requires system libraries (GTK,
  WebKitGTK) that may not be present in every build environment. The rest of the
  change is fully built and tested by the main CMake/Cargo workflow.
- Rationale: keeps the mandatory test matrix green while still adding the panel
  source and a documented build recipe.

### Reuse ABI-1 and existing generated contract bindings

- New contracts (`BlackboardValue`, `BlackboardUpdate`, `ScenarioState`,
  `ScenarioCommand`) are declared in YAML, validated by `nxdc`, and emitted as
  C++23 POD and Rust `#[repr(C)]` types. The component uses the existing
  `nxd-sdk` safe wrappers.
- Rationale: one source of truth for layouts and `layout_hash`; no ABI bump
  needed.

## Risks / Trade-offs

- **Panel native dependencies**: Tauri needs WebKitGTK/GTK3 on Linux. If these
  are missing, the panel cannot be built, but the core change remains testable.
  Mitigation: panel is excluded from the default workspace; a separate
  `cargo tauri build` step is documented.
- **Multi-process single-writer enforcement**: iceoryx2 does not enforce a
  single writer by itself. The runtime will document that each entry has one
  designated writer and rely on plan validation to guarantee it.
- **Asynchronous commands vs. determinism**: panel commands arrive asynchronously.
  The component applies them only at the declared `apply_frame_no`, preserving
  deterministic frame identity.
- **Spec overlap with transport**: `BlackboardUpdate` events may be carried by
  the transport layer. The delta specs keep Blackboard semantics distinct from
  port-sample semantics.

## Migration Plan

1. Create the delta specs and design under
   `openspec/changes/add-blackboard-rust-and-tauri-skeleton/`.
2. Add Blackboard contracts and regenerate bindings.
3. Implement `runtime/blackboard` headless backend and layout tests.
4. Implement `scenario_control` Rust component, descriptor, and build wiring.
5. Implement C++ test consumer and host integration tests.
6. Add Rust allocation-after-arm tests for `scenario_control`.
7. Add `panels/scenario-panel` Tauri + Svelte source and a backend-only test.
8. Run `ctest` (Debug/Release/ASan) and `cargo test --workspace` and archive the
   change after validation passes.
