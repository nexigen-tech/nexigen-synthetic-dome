# Spec Delta

## Purpose

Provides deterministic, lossless recording of typed transport samples and blackboard updates to a durable MCAP file for later audit, replay, and regression analysis.

## ADDED Requirements

### Requirement: Recorder subscribes to any transport topic by source name

The Recorder SHALL subscribe to one or more transport topics by source name and receive every sample published on those topics in publication order.

#### Scenario: Subscription captures all samples on a topic
- **WHEN** a source publishes ten samples on a subscribed topic during a run
- **THEN** the recording contains exactly ten samples for that topic in publication order

#### Scenario: Non-subscribed topics are ignored
- **WHEN** a source publishes samples on a topic that is not in the recorder subscription list
- **THEN** the recording contains no samples for that topic

### Requirement: Recorder persists samples with simulation-time metadata

The Recorder SHALL write every received sample to the MCAP file together with its simulation timestamp, transport sequence number, source name, and contract schema identifier.

#### Scenario: Recorded sample preserves timing and ordering
- **WHEN** a CAN signal source publishes a sample at simulation time 1.250 s with transport sequence 7
- **THEN** the recording stores the sample with simulation time 1.250 s, transport sequence 7, and the original source name

#### Scenario: Schema identifier is stored with each channel
- **WHEN** the recorder starts and subscribes to a topic carrying `SignalValue` samples
- **THEN** the MCAP file registers a channel whose schema identifier points to the canonical `SignalValue` schema record

### Requirement: Recorder writes exactly one MCAP file per run

The Recorder SHALL create a single MCAP file per recording run, close it gracefully when the run ends, and report the final file path and sample counts.

#### Scenario: Run ends with a closed file
- **WHEN** the recorder is started, samples are published, and the run is stopped
- **THEN** the MCAP file is closed, readable by standard MCAP tools, and contains metadata summarizing total samples per topic

#### Scenario: Empty run produces a valid empty recording
- **WHEN** the recorder starts and stops with no samples published
- **THEN** it produces a valid MCAP file with zero messages and a run metadata attachment

### Requirement: Recorder records blackboard updates as a pseudo-topic

The Recorder SHALL support recording blackboard update events as a special topic so that validation rules and replay can observe state changes.

#### Scenario: Blackboard state change is recorded
- **WHEN** a component writes a key-value update to the blackboard during recording
- **THEN** the recording contains the update as a sample on the reserved blackboard topic with the simulation time of the write
