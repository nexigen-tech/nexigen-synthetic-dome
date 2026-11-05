# Spec Delta

## Purpose

Provides a deterministic, hardware-faithful virtual CAN logical source for synthetic environments without requiring physical CAN controllers.

## ADDED Requirements

### Requirement: Virtual source publishes one RawCanBatch per simulation millisecond
The virtual CAN source SHALL emit exactly one `RawCanBatch` per base simulation frame on its assigned source name, carrying all CAN frames whose scheduled send time falls within that millisecond.

#### Scenario: Two frames scheduled in one millisecond
- **WHEN** two frames are scheduled at 1.000 s and 1.0005 s
- **THEN** the batch for frame 1000 contains both frames ordered by arbitration timestamp

#### Scenario: No frame scheduled in a millisecond
- **WHEN** no frame is scheduled in a given base frame
- **THEN** the source emits an empty `RawCanBatch` with `count == 0` as a completion watermark

### Requirement: Virtual arbitration follows ascending CAN identifier order
The virtual source SHALL order coincident frames by ascending CAN identifier, and for equal identifiers by the configured node priority order, matching wire-priority semantics.

#### Scenario: Two nodes send at the same simulation time
- **WHEN** node 11 sends `0x181` and node 12 sends `0x281` at the same timestamp
- **THEN** `0x181` appears before `0x281` in the batch

### Requirement: Timestamps reflect hardware-like nanosecond offsets
The virtual source SHALL assign deterministic nanosecond timestamps within the millisecond interval based on arbitration order, without using wall-clock or introducing jitter.

#### Scenario: Three ordered frames fit inside one millisecond
- **WHEN** three frames are ordered inside a 1 ms virtual frame
- **THEN** their timestamps are strictly increasing and bounded by the frame interval

### Requirement: Self-transmitted frames return as SelfRx
Frames originating from the virtual source itself SHALL appear in the emitted batch with `direction == SelfRx`, preserving the original CAN identifier, payload, and timestamp.

#### Scenario: Loopback frame reaches subscribers
- **WHEN** the virtual source transmits a frame
- **THEN** the same frame appears in the next batch with `direction == SelfRx`

### Requirement: Source-name wiring isolates independent CAN buses
The virtual source SHALL bind to a declared logical source name; unrelated source names SHALL NOT receive its batches.

#### Scenario: CAN_A and CAN_B are wired separately
- **WHEN** a component subscribes to source name `CAN_B`
- **THEN** it never receives batches published on `CAN_A`

### Requirement: Hot path performs no heap allocation
After successful arming, the virtual source SHALL not allocate heap memory, acquire locks, parse configuration, or create threads while producing batches.

#### Scenario: Continuous virtual transmission is allocation-free
- **WHEN** the source is armed and stepped repeatedly
- **THEN** per-step instrumentation reports zero heap allocations
