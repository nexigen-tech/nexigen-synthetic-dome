# panels/scenario-panel Specification

## Purpose
`scenario_panel` is a separate-process operator panel that lets a human or test
script send allowlisted scenario commands and observe decimated Blackboard
snapshots. It proves the Tauri + Svelte panel architecture without putting any
panel logic inside the deterministic simulation process.

## Requirements

### Requirement: Panel is a separate OS process

`scenario_panel` SHALL run in its own OS process and SHALL NOT be loaded as a
component plugin by the simulation host.

#### Scenario: Panel starts after the simulation

- **WHEN** `nexigen-run` has launched the simulation processes
- **THEN** the panel process can connect to the shared Blackboard/transport
  backend independently

### Requirement: Panel backend uses the same Blackboard/iceoryx2 backend

The Tauri Rust backend SHALL subscribe to Blackboard entries and publish
`ScenarioCommand` events through the same backend used by `runtime/blackboard`
and `runtime/transport`.

#### Scenario: Panel command reaches `scenario_control`

- **WHEN** the operator clicks a command button in the panel
- **THEN** a `ScenarioCommand` event is published and `scenario_control` receives
  it

### Requirement: Panel frontend receives only decimated snapshots

The Svelte frontend SHALL receive small, decimated snapshots of Blackboard
entries (for example `dome/info`, `scenario/state`, `video0/config`). It SHALL
NOT receive high-rate port samples or video frames.

#### Scenario: Front-end shows scenario state

- **WHEN** `scenario/state` changes
- **THEN** the panel UI updates with the latest snapshot at a decimated rate,
  not once per simulation frame

### Requirement: Closing the panel does not affect simulation

When the panel process terminates, the simulation SHALL continue with identical
frame results and output hashes.

#### Scenario: Panel is closed mid-run

- **WHEN** the panel window is closed while the simulation is running
- **THEN** `scenario_control` continues stepping, `scenario/state` keeps its last
  value, and downstream outputs are unchanged

### Requirement: Panel sends only allowlisted commands

The panel UI SHALL expose only commands that are present in the scenario
allowlist configured for the current run.

#### Scenario: Unknown command is not sent

- **WHEN** the operator interacts with a control that is not in the allowlist
- **THEN** the panel does not publish a `ScenarioCommand` for that control
