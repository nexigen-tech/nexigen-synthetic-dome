# Component Host Delta Specification

## Purpose

Extends the baseline `components/host` specification with runtime integration points needed by Chronos, partitions, and LET boundaries. Components that do not declare runtime support continue to work through the existing host lifecycle.

The baseline specification remains authoritative for plugin boundary, ownership, lifecycle ordering, Infrastructure integration, ThreadSpec application, hot-path rules, and failure handling.

## Requirements

### Requirement: Runtime registration before arming

The host SHALL allow a component to register with the Chronos runtime during initialization and SHALL reject runtime registration changes after arming.

#### Scenario: Component registers a time callback

- **WHEN** a component declares runtime support and registers a frame-context callback during initialization
- **THEN** the host forwards the registered callback to Chronos before the first frame

#### Scenario: Registration after arm is rejected

- **WHEN** a component attempts to register a runtime callback after arming
- **THEN** the host returns a failure status and does not modify the armed configuration

### Requirement: Frame context includes simulation time

The frame context passed to a component step SHALL include frame number, `SimTime`, `WallTime`, and `HwFpgaTime` (when available) in addition to the existing logical values, typed ports, and telemetry handles.

#### Scenario: Component reads SimTime from frame context

- **WHEN** a stepped component reads the simulation time from its frame context
- **THEN** it observes the SimTime published by Chronos for the current frame

### Requirement: Execution mode transitions are observable

The host SHALL notify components of Chronos state transitions (`initialize`, `arm`, `play`, `pause`, `step`, `stop`, `dispose`) through opt-in callbacks and SHALL guarantee that no step callback executes concurrently with a transition callback.

#### Scenario: Component reacts to pause

- **WHEN** Chronos transitions from play to pause
- **THEN** the component receives a pause callback before any subsequent step or play callback

### Requirement: Partition membership is declared

A component descriptor SHALL declare the partition to which the component belongs. The host SHALL reject a configuration in which the same component instance is assigned to more than one partition.

#### Scenario: Component in default partition

- **WHEN** a descriptor does not declare a partition
- **THEN** the host places the component in the default partition and drives it with the default Chronos configuration

### Requirement: Host enforces read-only frame context

The host SHALL provide the frame context as a borrowed, read-only value for the duration of the step. A component SHALL NOT retain a pointer to the frame context after the step returns.

#### Scenario: Component copies time values

- **WHEN** a component needs time values after the step
- **THEN** it copies the values into its own state during the step and does not retain the context pointer
