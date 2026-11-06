# Spec Delta

## Purpose

Defines stable contracts for recording metadata, replay control commands, and validation results so that recording, replay, and validation services can interoperate across runtime implementations and language bindings.

## ADDED Requirements

### Requirement: Recording metadata contract identifies the run and recorded channels

The `RecordingMetadata` contract SHALL identify the recording run by a unique identifier, record the start simulation time, list every recorded channel with its source name and schema identifier, and report total sample counts per channel.

#### Scenario: Metadata captures run identity and channels
- **WHEN** a recording run completes with channels `can0` and `video0`
- **THEN** the `RecordingMetadata` attachment contains a unique run id, start simulation time, and entries for `can0` and `video0` with their schema identifiers and message counts

#### Scenario: Empty channel is still listed
- **WHEN** a channel is subscribed but produces no samples during the run
- **THEN** the metadata lists the channel with a count of zero

### Requirement: Replay control contract carries runtime playback commands

The `ReplayControl` contract SHALL carry commands to start, pause, resume, stop, seek, set playback speed, and enable or disable looping, along with the target simulation time where applicable.

#### Scenario: Pause command
- **WHEN** a `ReplayControl` message with command `PAUSE` is sent to the replay service
- **THEN** the replay service pauses playback at the current simulation time

#### Scenario: Seek command
- **WHEN** a `ReplayControl` message with command `SEEK` and target time 5.000 s is sent
- **THEN** the replay service resumes emission from the first sample at or after simulation time 5.000 s

### Requirement: Validation result contract records rule outcomes

The `ValidationResult` contract SHALL record the run outcome, the list of evaluated rules, the number of passes and failures, and per-failure details including rule name, simulation time, and a human-readable reason.

#### Scenario: Failed threshold result
- **WHEN** a threshold rule named `speed-limit` fails at simulation time 2.300 s with offending value 120
- **THEN** the `ValidationResult` contains an `OUTCOME_FAIL` overall outcome and a failure entry for `speed-limit` at 2.300 s with value 120 and reason "value exceeds threshold 100"

#### Scenario: Successful run result
- **WHEN** all rules pass during the run
- **THEN** the `ValidationResult` contains `OUTCOME_PASS`, the rule count, a pass count equal to the number of evaluated samples, and zero failures

### Requirement: Contracts use the canonical schema format

All recording contracts SHALL be defined using the project's canonical schema format and registered in the schema registry so that transport serialization, code generation, and MCAP schema records refer to the same definitions.

#### Scenario: Schema registration
- **WHEN** the recording subsystem starts
- **THEN** the schemas for `RecordingMetadata`, `ReplayControl`, and `ValidationResult` are registered under stable identifiers and versions
