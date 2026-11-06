# runtime/blackboard Specification

## Purpose
Provides slow shared state for configuration, scenario state, source catalog, and
Dome metadata. The Blackboard is not a high-rate signal channel; it is a
single-writer, versioned, bounded-POD store that C++ and Rust components can
read and write deterministically within a frame.

## Requirements

### Requirement: Blackboard entries are single-writer versioned POD values

Each Blackboard entry SHALL have exactly one writer, a monotonic `revision`, an
`apply_frame_no`, a `generation`, and a versioned POD value identified by its
contract name and `layout_hash`.

#### Scenario: C++ component reads a Rust-written entry

- **WHEN** `scenario_control` writes `scenario/state` with `apply_frame_no = N`
- **THEN** a C++ consumer reading the entry after frame `N` sees the new
  `revision` and value

### Requirement: Blackboard supports headless and iceoryx2 backends

The Blackboard runtime SHALL provide a headless in-process backend for tests and
an iceoryx2 backend for multi-process panels. Both backends SHALL expose the
same C++ API and value layout.

#### Scenario: Headless test runs without iceoryx2

- **WHEN** a test creates a headless Blackboard and registers two components
- **THEN** both components read and write entries without launching iceoryx2
  services

### Requirement: Blackboard hot path is allocation-free

After arming, Blackboard read and write operations SHALL not allocate heap
memory, acquire mutexes, or throw exceptions.

#### Scenario: Stepping `scenario_control` is allocation-free

- **WHEN** `scenario_control` is armed and stepped repeatedly
- **THEN** allocation instrumentation reports zero heap allocations in
  Blackboard operations

### Requirement: Blackboard changes emit `BlackboardUpdate` events

Every successful write SHALL produce a `BlackboardUpdate` event containing the
entry name, `revision`, `apply_frame_no`, `generation`, contract name, and
`layout_hash` so that later recording/replay can observe the change.

#### Scenario: Recording observes a scenario state change

- **WHEN** `scenario_control` writes `scenario/state`
- **THEN** a subscriber receives a `BlackboardUpdate` whose `revision` and
  `apply_frame_no` match the written entry

### Requirement: Blackboard entries are isolated by name

Reads and writes SHALL be routed by entry name. Entries with different names
SHALL NOT alias or share storage.

#### Scenario: Two source entries are independent

- **WHEN** `source/CAN_A` and `source/Camera_Front_Left` both exist
- **THEN** writing `source/CAN_A` does not change `source/Camera_Front_Left`

### Requirement: Blackboard readers validate layout hash before use

A reader SHALL compare the entry's `layout_hash` with the expected contract
layout and SHALL reject the read if they differ.

#### Scenario: Layout mismatch is rejected

- **WHEN** a C++ component reads `scenario/state` but the entry value has a
  different `layout_hash` than the generated `ScenarioState` type
- **THEN** the read fails with a layout-mismatch diagnostic
