# Spec Delta

## Purpose

Provides a generic CAN observing capability that translates raw batches, heartbeats, and aggregate activity into stable signals for panels and recording.

## ADDED Requirements

### Requirement: Monitor subscribes by logical source name
A CAN monitor component SHALL declare an input port bound by logical source name, not by physical channel, and shall receive only batches published on that source name.

#### Scenario: CAN_A monitor ignores CAN_B
- **WHEN** a monitor subscribes to source name `CAN_A`
- **THEN** batches published on `CAN_B` do not appear at its input port

#### Scenario: Same monitor reused for different source
- **WHEN** a second monitor instance is configured with source name `CAN_B`
- **THEN** it receives only batches published on `CAN_B`

### Requirement: Monitor emits CanHeartbeat for silence detection
The monitor SHALL emit one `CanHeartbeat` when a configured silence interval expires since the last received frame, exposing hardware-like inactivity timing through simulation frame identity.

#### Scenario: No frames in 10 ms produce heartbeat
- **WHEN** no CAN frame arrives for 10 ms
- **THEN** the monitor emits a `CanHeartbeat` whose `hw_time_ns` reflects the expected silence boundary

### Requirement: Monitor computes CanAggregateErrors from observed batches
The monitor SHALL accumulate reception counters and error flags from observed `RawCanBatch` entries and publish a `CanAggregateErrors` once per base frame.

#### Scenario: Degraded frame increments degraded counter
- **WHEN** a batch contains one frame with `status == Degraded`
- **THEN** the emitted `CanAggregateErrors` has `rx_degraded == 1` and `status == Degraded`

#### Scenario: CRC error frame is preserved in counts
- **WHEN** a batch contains a frame with CRC error flag set
- **THEN** `crc_errors` is incremented and the frame payload is not discarded from the aggregate

### Requirement: Panel receives decimated updates
The monitor SHALL provide a small panel interface that forwards decimated state snapshots, not raw batches, so that UI updates do not dominate the hot path.

#### Scenario: Panel receives snapshot every fifth frame
- **WHEN** the panel is connected and batches arrive every millisecond
- **THEN** the panel receives one update every 5 ms containing counters and latest status flags

### Requirement: Monitor output is deterministic for the same input sequence
Given identical configuration, source wiring, and frame sequence, the monitor SHALL produce identical heartbeat and aggregate outputs.

#### Scenario: Repeated trace matches
- **WHEN** the same input trace is replayed twice through the monitor
- **THEN** both runs emit the same sequence of `CanHeartbeat` and `CanAggregateErrors` values
