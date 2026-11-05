# Tasks

## 1. Virtual CAN source and bounded scheduler

- [x] 1.1 Create `components/virtual_can` directory with C++ component descriptor (`component.yaml`) declaring output port `out` on contract `RawCanBatchClassical`, source name parameter `source_name`, and thread role. Verify with `nxdc validate components/virtual_can/component.yaml`.
- [x] 1.2 Implement bounded send queue and deterministic scheduler that emits one batch per 1 ms simulation frame, sorted by CAN id. Verify with a headless test that two scheduled frames appear in order.
- [x] 1.3 Add SelfRx loopback so transmitted frames return in the same batch with `direction == SelfRx`. Verify with a headless test that a sent frame is observable as SelfRx.
- [x] 1.4 Add empty-batch watermark behavior and allocation-after-arm guard. The hot path uses only pre-allocated `std::array` storage; no heap allocation occurs after `arm`. Determinism is verified by byte-for-byte replay in `tests/host_virtual_can_test.cpp`.

## 2. XDD parser and object dictionary (deferred)

- [ ] 2.1 Choose and integrate an XML parser for cold-path XDD parsing; add dependency records. Verify build succeeds in Debug and Release.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 2.2 Implement XDD CiA 311 loader that extracts indexes/sub-indexes, defaults, PDO mapping entries, heartbeat producer interval, and SYNC period; reject duplicates and malformed entries. Verify with positive and negative fixture tests.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 2.3 Create immutable object dictionary data structure with bounded storage sized from XDD. Verify layout and size tests.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.

## 3. CANopen sans-IO skeleton (deferred)

- [ ] 3.1 Implement COB-ID filter derivation from OD and node ID. Verify filter unit tests for node 11 and node 12 ranges.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 3.2 Implement SYNC producer scheduling (5 ms period) and PDO response within 5 ms budget. Verify step-by-step frame sequence in headless harness.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 3.3 Implement PDO mapping application for TPDO/RPDO entries. Verify sensor-to-controller value round trip.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 3.4 Implement heartbeat producer (boot-up + periodic heartbeat) and heartbeat consumer status. Verify missed-heartbeat scenario produces degraded aggregate status.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 3.5 Ensure CANopen stack state machine uses no heap allocation after arming. Verify allocation-after-arm test.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.

## 4. Two-node skeleton components (deferred)

- [ ] 4.1 Create XDD files for `sensor_a` (node 11) and `controller_b` (node 12) defining TPDO/RPDO mappings and heartbeat intervals. Verify both files parse without errors.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 4.2 Implement `sensor_a` component wrapper that reads `controller_b` RPDO and publishes `controller_b` TPDO under SYNC. Verify with deterministic headless test.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 4.3 Implement `controller_b` component wrapper that reads `sensor_a` PDO and publishes its own PDO/heartbeat. Verify data reaches both ends of the loop.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.
- [ ] 4.4 Add descriptors for both components with file-valued XDD parameters and logical CAN ports. Verify descriptor validation passes.
  - **Status:** Deferred to follow-up change `add-canopen-skeleton`.

## 5. CAN monitor

- [x] 5.1 Create `components/can_monitor` descriptor with input port subscribed to `CAN_A` and output ports `CanHeartbeat` / `CanAggregateErrors`. Verify descriptor validation.
- [x] 5.2 Implement aggregate counters and error flags per `RawCanBatchClassical`. Verify degraded/CRC frames update counters correctly.
- [x] 5.3 Implement silence heartbeat generation using simulation time and configured interval. Verify heartbeat emits after configured silence.
- [x] 5.4 Add allocation-after-arm guard and headless deterministic replay test. Output buffers are written in-place; no heap allocation occurs after `arm`. Repeated traces match byte-for-byte in `tests/host_can_monitor_test.cpp`.

## 6. Composer plan fixtures and source-name isolation

- [x] 6.1 Create a walking-skeleton plan fixture binding `virtual_can` and `can_monitor` on source name `CAN_A`, with a 1 ms base frame. Verify Composer plan validation.
- [x] 6.2 Add source-name isolation test: a second virtual source `CAN_B` wired to a second monitor shall not deliver frames to the `CAN_A` monitor. Verify test passes.
- [x] 6.3 Create end-to-end skeleton test running a few deterministic frames and comparing the emitted CAN batches and monitor outputs across runs. Verify both runs match.

## 7. Verification and documentation

- [x] 7.1 Run C++ Debug, Release, ASan, UBSan, TSan, and Clang builds; fix sanitizer findings. Verify all builds pass.
- [x] 7.2 Run Rust debug/release builds and clippy checks for any Rust crates touched. Verify no warnings (pre-existing `nxdc` codegen warning remains).
- [x] 7.3 Update `docs/` or component-sdk docs if new descriptor patterns are introduced. `docs/component-sdk.md` now references the virtual source, monitor, and Composer source-name isolation.

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
- Verify implementation against the spec delta requirements before archiving.
