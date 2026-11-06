# runtime/validation Specification

## Purpose
Evaluates declarative predicates over live or replayed transport samples and blackboard updates, producing machine-readable pass or fail events and a final validation report for tests and audits.

## Requirements

### Requirement: Validation service subscribes to declared topics and blackboard keys

The Validation service SHALL subscribe to the transport topics and blackboard keys referenced by the validation rules declared for the current run.

#### Scenario: Rule on a topic receives the required samples
- **WHEN** a rule declares a predicate over topic `can0`
- **THEN** the validation service subscribes to `can0` and evaluates the predicate for each sample on that topic

#### Scenario: Rule on a blackboard key receives state updates
- **WHEN** a rule declares a predicate over blackboard key `/vehicle/speed`
- **THEN** the validation service observes every update of that key and evaluates the predicate against the new value

### Requirement: Validation rules evaluate predicates per sample

The Validation service SHALL evaluate each rule's predicate against the current sample or blackboard value and emit a validation event indicating pass or fail.

#### Scenario: Simple threshold passes
- **WHEN** a rule requires that every `SignalValue` on topic `can0` is less than 100
- **THEN** a sample with value 50 produces a pass event and a sample with value 150 produces a fail event

#### Scenario: Presence rule checks field existence
- **WHEN** a rule requires that every CAN frame contains the `timestamp_us` field
- **THEN** a sample with the field produces a pass event and a sample without it produces a fail event

### Requirement: Validation rules can compare samples against expectations

The Validation service SHALL support comparing a sample field to a constant value, another field, or a previously recorded baseline value.

#### Scenario: Field-to-constant comparison
- **WHEN** a rule requires that `state` equals `"active"`
- **THEN** a sample with `state == "active"` passes and a sample with `state == "idle"` fails

#### Scenario: Field-to-field comparison
- **WHEN** a rule requires that `measured` equals `expected`
- **THEN** a sample where both fields are equal passes and a sample where they differ fails

### Requirement: Validation service produces a final report

The Validation service SHALL produce a final report at the end of the run containing the outcome, evaluated rule count, pass count, fail count, and one failure record per failing rule with simulation time and offending value.

#### Scenario: All rules pass
- **WHEN** every rule predicate evaluates to true for all samples during the run
- **THEN** the final report outcome is `PASS` with zero failures

#### Scenario: A rule fails at a specific time
- **WHEN** a threshold rule fails at simulation time 3.500 s on topic `can0`
- **THEN** the final report contains an `OUTCOME_FAIL` entry with simulation time 3.500 s, the rule name, and the sample value that violated the predicate

### Requirement: Validation runs during live and replayed simulations

The Validation service SHALL be usable both during a live run and while replaying a recording, producing deterministic results for the same inputs.

#### Scenario: Live validation matches replay validation for the same recording
- **WHEN** a run is recorded, then replayed with the same validation rules
- **THEN** the validation reports from the live run and the replayed run are identical
