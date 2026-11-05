# Design

## Context

The previous changes established bounded CAN contracts, the component host, Chronos runtime, Composer validation, and a language-neutral plugin boundary. The repository therefore already provides:

- `RawCanFrame`, `RawCanBatchClassical`, `CanHeartbeat`, and `CanAggregateErrors` contracts;
- component descriptor schema with file-valued parameters, thread roles, and logical ports;
- `runtime/chronos` frame context and partition/LET semantics;
- `runtime/composer` manifest validation;
- `host/` C ABI for C++ and Rust plugins.

This change layers a virtual CAN source, a minimal CANopen stack, and a CAN monitor on top of that foundation without changing contracts or host ABI. See `proposal.md` for motivation.

## Goals / Non-Goals

**Goals:**

- Deliver a deterministic virtual CAN logical source (`CAN_A`) that publishes `RawCanBatchClassical` every 1 ms simulation frame.
- Provide a `can_monitor` component that subscribes to `CAN_A` and publishes `CanHeartbeat` plus `CanAggregateErrors`.
- Add Composer plan fixtures and tests that prove deterministic replay and source-name isolation.

**Non-Goals / Deferred:**

- The sans-IO CANopen CiA 301 stack, XDD parsing, and the two-node (`sensor_a`/`controller_b`) wrappers are deferred to a follow-up change (`add-canopen-skeleton`).
- Real PCIe/FPGA CAN gateway, CAN FD/XL wire semantics, and full CiA 301 conformance.
- Production-grade CANopenNode integration; the spike decides which minimal path to keep.
- Generic panel UI beyond a thin decimated snapshot interface.
- Distributed multi-node execution; the skeleton remains local multi-process.

## Decisions

### D1. Virtual CAN source is a stateless deterministic producer component

`virtual_can` is implemented as a component with one output port carrying `RawCanBatchClassical`. It owns a bounded send queue created during initialization. On each step it.

- receives the current simulation frame and `SimTime` from Chronos;
- collects frames whose scheduled send time falls in `[frame_time, frame_time + 1 ms)`;
- sorts them by CAN identifier (then by static node priority for ties);
- assigns virtual hardware timestamps as deterministic nanosecond offsets within the frame;
- emits exactly one `RawCanBatchClassical` with `count`, `assigned_frame`, and sorted frames.

This keeps the source independent of physical interfaces and lets the same component run in headless tests, SIL, or step mode.

### D2. SelfRx frames are produced by the same component instance

Frames transmitted by `virtual_can` are not echoed back through the transport. Instead, the component schedules a SelfRx copy with `direction == SelfRx` and emits both the original transmit and the loopback entry in the same deterministic batch. This matches real hardware behavior where the controller observes its own frame on the bus and removes transport-specific echo complexity from the monitor tests.

### D3. CANopen stack is split into sans-IO engine and thin component wrapper

The protocol logic lives in a reusable sans-IO library (name: `libnxd_canopen`). The library exposes:

- `NxdCanopenInit(xdd_path, node_id)` returning a cold-path descriptor/object dictionary;
- `NxdCanopenStep(od, input_batch, sim_time, output_callback)` producing zero or more frames;
- getters for heartbeat status, error state, and consumed/produced PDO payloads.

The component wrapper (`sensor_a`, `controller_b`) owns one engine instance, maps ports to engine callbacks, and ensures all pre-allocated buffers are sized from the XDD during initialization.

**Alternative considered:** vendor CANopenNode. Rejected for v1 because adapting its IO layer and allocation strategy to the zero-allocation-after-arm rule exceeds the skeleton scope; a spike will quantify the gap.

### D4. XDD parsing is cold-path and produces immutable object dictionary

XDD XML is parsed once during `initialize` using a streaming XML reader (`roxmltree` in Rust or `tinyxml2`/`pugixml` in C++). The parser builds:

- an index map `index:sub` → object metadata;
- PDO mapping tables for TPDO/RPDO COB-IDs;
- SYNC period, heartbeat producer time, and node-ID-related COB-ID offsets.

The resulting object dictionary is stored in bounded POD vectors. Parse errors abort initialization before arming, satisfying the descriptor spec for file-valued parameters.

### D5. PDO timing follows Chronos frame boundaries

The SYNC period is declared as 5 ms in the plan. The CANopen component schedules a SYNC frame when `sim_time % 5 ms == 0`. PDO responses are generated in the same step if the input batch contains the SYNC, or in the next 1 ms frame, guaranteeing the 5 ms response budget without real-time sleeps.

### D6. Source-name subscription is enforced by Composer partition wiring

Composer `Partition` declarations carry an optional `source_name` string that tags every instance in the partition with its logical bus (`CAN_A`, `CAN_B`, etc.).  The `validate_source_name_wiring` rule rejects any wire whose source and target instances belong to partitions with different, non-empty source names.  This isolates logical buses at plan-validation time without runtime filtering and makes source leakage a composition failure.

### D7. Monitor aggregates are computed per-step from the batch

`can_monitor` reads one `RawCanBatchClassical` per step and updates counters in pre-allocated POD. It emits `CanAggregateErrors` every frame and `CanHeartbeat` only when a configured silence interval has elapsed since the last received frame. Heartbeat generation uses the last observed simulation time, not wall clock, preserving replay determinism.

### D8. Walking skeleton uses existing Composer/Chronos plumbing

The skeleton reuses `runtime/composer` for validation and `runtime/chronos` for time distribution. The reduced-scope fixture declares:

- one partition/source `CAN_A` containing `virtual_can` and `can_monitor`;
- a direct wire from `virtual_can:out` to `can_monitor:in`;
- a 1 ms base frame interval.

No new runner is introduced; the tests use the existing headless harness and Composer plan fixtures.

## Risks / Trade-offs

- **CANopen stack is out of scope for this change** → the reduced skeleton defers CiA 301/XDD work; the follow-up `add-canopen-skeleton` change will quantify gaps for gateway work.
- **SelfRx timing may be confusing compared to real hardware echo latency** → document that virtual source echoes at virtual arbitration time; physical gateways will model real echo in a later change.
- **Source-name wiring is enforced at plan level** → runtime filtering is not implemented; cross-source leakage is prevented by Composer validation, which is sufficient for the skeleton.

## Migration Plan

1. Add `components/virtual_can` descriptor and implementation.
2. Add `components/can_monitor` descriptor and implementation.
3. Add Composer `source_name` field to `Partition` and `validate_source_name_wiring` rule.
4. Add Composer plan fixtures for `CAN_A` and source-name isolation (`CAN_A` vs `CAN_B`).
5. Add headless deterministic tests for `virtual_can` and `can_monitor`, plus replay checks.
6. Run C++ Debug/Release/ASan/TSan/UBSan/Clang and Rust debug/release suites.

Rollback removes the new component directories and CANopen library without affecting contracts, host ABI, or existing Chronos/Composer targets.

## Open Questions

- Which XML parsing library to use for XDD will be decided during implementation based on existing project dependencies and parser licensing.
- The exact file path for the `sensor_a`/`controller_b` XDD files and whether they are shared or per-component will be fixed in tasks.
