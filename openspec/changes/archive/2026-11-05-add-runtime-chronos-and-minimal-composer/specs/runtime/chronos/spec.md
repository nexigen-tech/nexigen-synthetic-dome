# Chronos Runtime Specification

## Purpose

Provides a deterministic simulation-time distribution service that separates logical simulation time from wall-clock and hardware-FPGA time, supports multiple execution modes, and gives every component a consistent time context within its partition.

Chronos is distinct from the host `nexigen::chrono` clock service: `nexigen::chrono` reads local host clocks, while Chronos owns the simulation timeline, distributes it to partitions, and enforces execution-mode policy.

## ADDED Requirements

### Requirement: Integer time representation

All Chronos time values SHALL be represented as signed or unsigned integers in nanoseconds and SHALL avoid floating-point arithmetic on the hot path.

#### Scenario: Time arithmetic is exact

- **WHEN** two SimTime values are subtracted to compute a frame budget
- **THEN** the result is exact and independent of host floating-point behavior

### Requirement: Three time domains

Chronos SHALL distinguish `SimTime` (logical simulation time), `WallTime` (host monotonic clock), and `HwFpgaTime` (common hardware time base when running HIL).

#### Scenario: Scaled SIL decouples SimTime from WallTime

- **WHEN** the system runs in Scaled SIL mode with a scale factor of 2
- **THEN** SimTime advances twice as fast as WallTime for that interval

#### Scenario: HIL uses HW_FPGA_TIME_BASE

- **WHEN** the system runs in RT/HIL mode
- **THEN** HwFpgaTime is the common hardware time base and the axis of HIL samples is derived from hardware stamps, not IRQ wake time

### Requirement: Execution modes

Chronos SHALL support at least RT/HIL, Scaled SIL, Free SIL, and Step modes, selected by policy before arming.

#### Scenario: Hardware component rejects virtual-time mode

- **WHEN** a component requiring real-time hardware operation is configured for Scaled, Free, or Step mode
- **THEN** validation rejects the configuration before component initialization

### Requirement: Base frame and fast-frame support

Chronos SHALL support a configurable base frame interval. The default base frame for v1 SHALL be 1 ms. Fast-frame intervals down to 100 µs SHALL be representable as an architectural option without requiring full v1 implementation.

#### Scenario: Default base frame is 1 ms

- **WHEN** a manifest omits an explicit base frame interval
- **THEN** Chronos uses 1 ms unless the configuration explicitly requests another supported interval

### Requirement: Time distribution to partitions

Chronos SHALL publish a frame context containing frame number, SimTime, WallTime, and HwFpgaTime (when available) to every configured partition at the start of each frame.

#### Scenario: All components in a partition see the same frame time

- **WHEN** two components belong to the same partition
- **THEN** both receive identical frame number and SimTime for the same frame

### Requirement: Logical Execution Time boundary

Cross-partition communication SHALL cross a LET boundary with a published read time and a write deadline. Direct communication SHALL be allowed only inside a partition.

#### Scenario: LET output is written before the deadline

- **WHEN** a component in partition A produces an output intended for partition B
- **THEN** the output is written by the LET write deadline and consumed by B at its published read time

#### Scenario: Direct cross-partition port is rejected

- **WHEN** a manifest wires two ports in different partitions directly
- **THEN** validation rejects the wiring unless a LET boundary is declared between the partitions

### Requirement: State control API

Chronos SHALL expose a state-control API supporting `initialize`, `arm`, `play`, `pause`, `step`, `stop`, and `dispose` states and transitions.

#### Scenario: Pause freezes SimTime

- **WHEN** the system is paused from play
- **THEN** SimTime stops advancing and components stop receiving new frames until play or step is requested

#### Scenario: Step advances one frame

- **WHEN** a step request is issued in Step mode
- **THEN** SimTime advances by exactly one base frame interval and exactly one frame context is distributed

### Requirement: Mode transitions are atomic and ordered

Chronos SHALL execute mode and state transitions atomically with respect to component frame execution and SHALL reject transitions that would violate the current mode policy.

#### Scenario: Arm rejected after play

- **WHEN** an arm request is issued while the system is in play
- **THEN** Chronos rejects the request and does not reinitialize an active timeline

### Requirement: Component time context is read-only

The frame context provided to a component SHALL be read-only for the duration of the frame. A component SHALL NOT modify the frame context or derive a different SimTime for the same frame.

#### Scenario: Component reads SimTime from context

- **WHEN** a component steps with a provided frame context
- **THEN** it reads SimTime, frame number, and Wall/HwFpga times from the context without allocation or synchronization

### Requirement: Deterministic replay prerequisites

Chronos SHALL produce enough information for a later replay to reconstruct the same SimTime sequence from the same configuration and initial conditions, independent of wall-clock jitter.

#### Scenario: Same manifest produces same SimTime sequence

- **WHEN** two runs use the same manifest, initial state, seed, and execution mode
- **THEN** the recorded SimTime and frame number sequences are identical

### Requirement: Hot path has no dynamic resource operations

After arming, Chronos frame distribution SHALL perform no heap allocation, JSON parsing, thread creation, plugin loading, or blocking I/O on the hot path.

#### Scenario: Empty partition steps repeatedly

- **WHEN** an empty partition is stepped repeatedly by Chronos
- **THEN** each frame publishes a frame context without additional allocation or synchronization beyond the configured distribution mechanism
