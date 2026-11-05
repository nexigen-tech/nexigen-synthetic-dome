# runtime/transport Specification

## Purpose
Provides a deterministic, zero-copy local transport that moves typed port samples between component instances using logical source names, with a test backend that does not require iceoryx2.

## Requirements

### Requirement: Transport binds logical source names to publishers and subscribers
The transport SHALL map each declared port to a logical source name and route samples only between publishers and subscribers registered for that name.

#### Scenario: Source CAN_A delivers only to CAN_A subscribers
- **WHEN** one publisher writes to source name `CAN_A` and two subscribers listen to `CAN_A` and `CAN_B`
- **THEN** the `CAN_A` subscribers receive the sample and the `CAN_B` subscriber does not

### Requirement: Transport supports headless test mode without iceoryx2
The transport SHALL provide an in-process channel backend that can be driven directly by the headless test harness using pre-allocated buffers.

#### Scenario: Headless harness drives two components
- **WHEN** the headless harness connects an output port of component A to an input port of component B with the same source name
- **THEN** every sample written by A in one step is visible to B in the next step without launching processes

### Requirement: Transport preserves frame and time identity
Every transported sample SHALL carry the simulation frame number and `SimTime` of the producing step so consumers can detect gaps or misalignment.

#### Scenario: Monitor detects a gap
- **WHEN** a consumer receives frames with numbers N and N+2
- **THEN** it can report a missing frame N+1 from the transport metadata

### Requirement: Transport performs no allocation after arming
Publishers and subscribers SHALL pre-allocate their loan pools and buffers during initialization; the hot path SHALL only acquire and publish existing loans.

#### Scenario: Steady-state transport is allocation-free
- **WHEN** the system is armed and samples flow between two processes
- **THEN** allocation instrumentation reports zero heap allocations in publisher and subscriber paths

### Requirement: Transport lifecycle matches component lifecycle
Publishers and subscribers SHALL be created during initialization and destroyed during stop; no transport endpoint is created or destroyed after arming.

#### Scenario: Stop releases transport resources
- **WHEN** the system is stopped
- **THEN** all transport loans are returned and endpoints are closed before component state is destroyed
