# components/scenario-control Specification

## Purpose
`scenario_control` is a Rust component that owns the `scenario/state` Blackboard
entry and applies operator commands at deterministic frames. It proves that a
Rust `cdylib` plugin can participate in the host lifecycle, write shared state,
and remain allocation-free after arming.

## Requirements

### Requirement: `scenario_control` is a Rust ABI-1 `cdylib` plugin

`scenario_control` SHALL be built as a `cdylib` using `nxd-sdk`, `#![no_std]`,
and `panic = "abort"`, and SHALL export the ABI-1 entry table.

#### Scenario: Host loads the Rust plugin

- **WHEN** the host loads `libnxd_scenario_control.so`
- **THEN** descriptor validation, ABI checks, and lifecycle calls succeed

### Requirement: `scenario_control` is the single writer of `scenario/state`

`scenario_control` SHALL be the only component that writes the
`scenario/state` Blackboard entry.

#### Scenario: Another component attempts to write `scenario/state`

- **WHEN** a second component tries to open `scenario/state` as a writer
- **THEN** the runtime rejects the write with a single-writer violation

### Requirement: `scenario_control` applies commands at `apply_frame_no`

The component SHALL accept `ScenarioCommand` input events and apply the command
value to `scenario/state` exactly on the frame specified by the command's
`apply_frame_no`.

#### Scenario: Command is applied on the expected frame

- **WHEN** a command with `apply_frame_no = 7` and value `42` is received
- **THEN** `scenario/state` has value `42` starting at frame `7`

### Requirement: `scenario_control` supports an allowlist via config

During `initialize`, the component SHALL load an allowlist of scenario commands
from a config parameter or file. Commands not in the allowlist SHALL be ignored.

#### Scenario: Allowlist rejects an unknown command

- **WHEN** a command with an unknown `command_id` arrives
- **THEN** the component does not change `scenario/state` and returns success

### Requirement: `scenario_control` has no allocation after arm

After `arm`, the component SHALL not perform any heap allocation while
processing commands or writing the Blackboard.

#### Scenario: Allocation-after-arm test fails the step

- **WHEN** a test allocator is enabled and the component allocates after `arm`
- **THEN** the step returns an error or the test process aborts

### Requirement: `scenario_control` emits `BlackboardUpdate` events

Every change to `scenario/state` SHALL produce a `BlackboardUpdate` event with
the entry name, `revision`, `apply_frame_no`, `generation`, and `layout_hash`.

#### Scenario: Update event matches the written state

- **WHEN** the component writes `scenario/state` with `revision = 3`
- **THEN** the emitted `BlackboardUpdate` has `revision = 3`
