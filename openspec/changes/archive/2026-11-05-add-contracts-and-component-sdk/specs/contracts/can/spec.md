# Spec Delta

## Purpose

Defines bounded, hardware-faithful CAN frame and batch contracts for protocol models, monitors, gateways, and recording without imposing CANopen semantics on every CAN variant.

## ADDED Requirements

### Requirement: Classical, FD, and XL storage variants

CAN contracts SHALL support Classical CAN, CAN FD, and CAN XL with bounded payload capacities of 8, 64, and 2048 bytes respectively and an explicitly bounded frame count per batch.

#### Scenario: Storage matches protocol needs

- **WHEN** contracts are instantiated for Classical CAN, mixed Classical/FD, or CAN XL traffic
- **THEN** their maximum payload storage is respectively 8, 64, or 2048 bytes per frame and total batch memory is determinable before arming

#### Scenario: Oversized payload is rejected

- **WHEN** a producer attempts to place more bytes than the selected frame or batch capacity permits
- **THEN** the operation reports a capacity error without writing beyond the bounded storage

### Requirement: Hardware identity and reception status

Every received CAN frame SHALL carry a nanosecond hardware timestamp, direction (`Rx` or `SelfRx`), kind (`Data`, `Remote`, `Error`, or `Heartbeat`), and status (`Valid`, `Degraded`, or `Invalid`).

#### Scenario: Self-received frame retains identity

- **WHEN** hardware returns a frame originally transmitted by the same controller
- **THEN** the frame is represented with `SelfRx`, its original hardware timestamp, and its applicable protocol metadata

### Requirement: Raw DLC and actual payload length are distinct

Each CAN frame SHALL preserve the raw DLC value separately from the actual payload byte count and SHALL retain protocol-specific metadata without forcing CANopen addressing onto all protocols.

#### Scenario: Remote frame has DLC but no payload

- **WHEN** a Classical CAN remote frame is represented
- **THEN** its raw DLC remains available while its payload byte count is zero

#### Scenario: CAN XL is not interpreted as CANopen

- **WHEN** a CAN XL frame is inspected by a generic monitor
- **THEN** its protocol-specific metadata remains available without applying COB-ID ownership or CANopen field semantics

### Requirement: Reception errors remain observable

CAN frames SHALL support aggregate reception error flags and a vendor error code, and erroneous frames SHALL remain representable for monitors and recording.

#### Scenario: CRC-error frame reaches a monitor

- **WHEN** hardware reports a frame with a CRC mismatch
- **THEN** the raw frame retains its payload, timestamp, `Invalid` status, CRC error flag, and vendor error code instead of being silently discarded

### Requirement: Consumers can filter unsuitable frames

Protocol consumers SHALL be able to ignore heartbeats, unsupported protocols, and invalid data without losing the original raw stream for activity models and diagnostics.

#### Scenario: CANopen consumer ignores heartbeat

- **WHEN** a CANopen consumer evaluates a batch containing a heartbeat
- **THEN** it performs no PDO or SDO processing for that entry while an activity-aware consumer can still inspect it

### Requirement: Heartbeat preserves hardware inactivity timing

A CAN heartbeat SHALL carry its hardware formation timestamp and SHALL be distinguishable from an empty data frame.

#### Scenario: Model computes a silent interval

- **WHEN** an activity-aware model has a previous data timestamp `D` and receives a heartbeat timestamp `H`
- **THEN** it can derive the hardware-timed inactivity interval `[D, H)` independently of host delivery time

### Requirement: Batch completion is explicit

A raw CAN batch SHALL identify its assigned frame and frame count, and an empty batch SHALL remain representable as a completion watermark.

#### Scenario: Empty batch completes a frame

- **WHEN** no CAN frames are assigned to a simulation frame
- **THEN** an empty batch can still communicate that no additional entries for that frame will arrive
