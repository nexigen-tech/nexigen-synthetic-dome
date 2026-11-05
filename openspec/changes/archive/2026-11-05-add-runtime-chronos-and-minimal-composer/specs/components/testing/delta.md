# Component Testing Delta Specification

## Purpose

Extends the baseline `components/testing` specification with runtime-aware verification fixtures. The headless harness gains a deterministic fake Chronos source so component and partition behavior can be tested without launching processes or requiring real-time threads.

The baseline specification remains authoritative for frame-driven headless execution, deterministic repeated execution, allocation-after-arm detection, cross-language lifecycle equivalence, panic isolation, performance reporting, and build-mode coverage.

## Requirements

### Requirement: Fake Chronos source for headless tests

The test harness SHALL provide a fake Chronos source that can publish a configurable sequence of frame contexts containing frame number, `SimTime`, `WallTime`, and `HwFpgaTime`.

#### Scenario: Test supplies a time sequence

- **WHEN** a test configures the fake Chronos source with three frames at 1 ms intervals
- **THEN** the component under test receives three frame contexts with matching frame numbers and SimTime values

### Requirement: Deterministic time progression

The fake Chronos source SHALL advance time by exact, configurable increments and SHALL produce identical frame contexts for identical test configurations.

#### Scenario: Repeated time sequence matches

- **WHEN** the same test configuration is executed twice
- **THEN** every frame context in both runs is identical

### Requirement: LET boundary simulation

The test harness SHALL simulate LET boundaries between partitions by enforcing read times and write deadlines on cross-partition port traffic.

#### Scenario: Late LET write is detected

- **WHEN** a component writes to a cross-partition port after its LET write deadline
- **THEN** the harness records a deadline violation and the downstream partition does not observe the late value at the published read time

### Requirement: Mode transition test scenarios

The harness SHALL expose initialize, arm, play, pause, step, and stop transitions to tests and SHALL verify that components observe transitions in order without interleaving with step callbacks.

#### Scenario: Pause interrupts stepping

- **WHEN** a test issues pause after frame N
- **THEN** the component receives no frame N+1 before the pause transition callback completes

### Requirement: Runtime-aware allocation-after-arm detection

Allocation-after-arm detection SHALL apply during runtime callbacks and frame-context handling, not only during the component step function.

#### Scenario: Allocation in mode callback fails test

- **WHEN** a test component allocates memory inside a play/pause transition callback after arming
- **THEN** the harness records the allocation violation and fails the test

### Requirement: Deterministic replay fixture

The harness SHALL provide a fixture that records a sequence of frame contexts and LET boundary events and can replay it later for regression testing.

#### Scenario: Recorded context sequence replays identically

- **WHEN** a recorded sequence is replayed to the same component with the same initial state
- **THEN** every output and state hash matches the original run
