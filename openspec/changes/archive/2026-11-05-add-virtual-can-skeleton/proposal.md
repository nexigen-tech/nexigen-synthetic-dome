# Proposal

## Why

The project already has bounded CAN contracts, a language-neutral component host, and the Chronos runtime with partition/LET semantics. The next step is to prove that a protocol model can run deterministically inside the synthetic environment. A virtual CAN source and a minimal CANopen skeleton provide a self-contained, repeatable example that exercises time distribution, partition boundaries, source-name subscriptions, and monitor panels without requiring real hardware or a full field bus stack.

## What Changes

- Add a deterministic `virtual_can` logical source that publishes one `RawCanBatchClassical` per simulation millisecond on source name `CAN_A`, including empty batches as frame-completion watermarks.
- Implement virtual CAN arbitration, hardware-like nanosecond timestamps, and SelfRx loopback so that a frame transmitted by `virtual_can` returns to the same source exactly as real hardware would.
- Add a `can_monitor` component that subscribes to `CAN_A` by source name and exposes `CanHeartbeat` plus `CanAggregateErrors` for activity and error scenarios.
- Extend Composer `Partition` with an optional `source_name` field and add `validate_source_name_wiring` to reject cross-source wires at plan-validation time.
- Add Composer plan fixtures for a single `CAN_A` source/monitor pair and a `CAN_A`/`CAN_B` isolation scenario.
- Add headless component tests and replay checks that prove deterministic stepping for both components.

**Out of scope (v1):** CANopen CiA 301 stack, XDD parsing, `sensor_a`/`controller_b` nodes, generic panel UI, CAN FD/XL wire semantics, real PCIe/AltaDT/Kvaser gateways, full CANopen device-profile support, and deterministic HIL timestamp capture. The CANopen work is deferred to a follow-up change.

## Capabilities

### New Capabilities

- `components/virtual_can`: Logical CAN source `CAN_A`; deterministic millisecond-rate `RawCanBatchClassical` emission; virtual arbitration; hardware-like timestamps and SelfRx loopback; source-name parameter.
- `components/can_monitor`: `RawCanBatchClassical` consumer; aggregate error counters; silence heartbeat generation; source-name parameter.
- `runtime/composer`: Optional `source_name` on `Partition` and `validate_source_name_wiring` rule for source isolation.

### Modified Capabilities

- None for v1. Existing contracts and descriptor file-valued parameters are reused as-is.

## Impact

- **New systems:** `sources/virtual-can` component, `protocols/canopen` library, `monitors/can` component and panel, walking-skeleton scenario and tests.
- **Build:** new C++ and/or Rust targets for the virtual source, CANopen stack, and monitor; new component descriptors and generated bindings; new test fixtures.
- **Dependencies:** `contracts/can` (`RawCanFrame`, `RawCanBatch`, `CanHeartbeat`, `CanAggregateErrors`), `runtime/chronos` frame distribution, Composer plan validation, component host lifecycle.
- **Compatibility:** no breaking changes to existing contracts or host ABI; new components declare new logical ports and parameters.
- **Hot-path rules:** protocol processing consumes per-frame budgets with zero heap allocation after `Arm`; XDD parsing and OD construction happen during initialization.
- **Follow-on changes:** hardware CAN gateway, CAN FD/CAN XL semantics, recorder/replay of CAN streams.