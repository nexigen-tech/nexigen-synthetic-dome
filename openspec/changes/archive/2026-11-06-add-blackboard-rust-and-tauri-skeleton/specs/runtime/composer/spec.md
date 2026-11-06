# Spec Delta

## ADDED Requirements

### Requirement: Dome Plan may declare Blackboard seed entries

A Dome Plan SHALL be able to list initial Blackboard entries (`dome/info`,
`source/*`, `scenario/state`, `video*/config`) with their contract name,
`layout_hash`, default value, and designated writer instance so that the runtime
can create them before arming.

#### Scenario: Plan seeds `scenario/state`

- **WHEN** a Dome Plan contains a `blackboard_seed` section with `scenario/state`
- **THEN** the runtime creates the entry with the given default value before any
  component is armed

### Requirement: Dome Plan may declare panel processes

A Dome Plan SHALL be able to declare optional panel processes (for example
`scenario_panel`) with their executable path and allowed Blackboard subscriptions
so that `nexigen-run` can launch them alongside simulation processes.

#### Scenario: Plan includes `scenario_panel`

- **WHEN** a Dome Plan declares a panel process for `scenario_panel`
- **THEN** `nexigen-run` can launch it as a separate OS process connected to the
  same Blackboard backend
