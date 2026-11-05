# Spec Delta

## Purpose

Provides a minimal deterministic CANopen CiA 301 protocol stack for synthetic models, driven by XDD description and constrained to a two-node walking skeleton.

## ADDED Requirements

### Requirement: XDD object dictionary is parsed on the cold path
The CANopen stack SHALL parse the provided XDD CiA 311 file during component initialization and build an immutable object dictionary, reporting parse errors before arming.

#### Scenario: Valid XDD produces indexes and mappings
- **WHEN** a valid XDD file is supplied at initialization
- **THEN** the stack exposes PDO mappings, heartbeat producer time, and SYNC parameters with stable numeric identifiers

#### Scenario: Invalid XDD rejects initialization
- **WHEN** an XDD file with duplicate indexes or malformed entries is supplied
- **THEN** initialization fails with a diagnostic naming the offending entry

### Requirement: COB-ID filters are derived from object dictionary
The stack SHALL derive receive COB-ID filters from the configured node IDs and PDO mappings so that only relevant frames traverse the protocol state machine.

#### Scenario: PDO for another node is ignored
- **WHEN** a frame with a COB-ID belonging to node 12 is observed while configured as node 11
- **THEN** node 11 does not process that frame as its own PDO

### Requirement: SYNC producer triggers periodic PDO exchange
The stack SHALL generate SYNC messages every 5 ms when configured as SYNC producer, and PDO producers SHALL respond within the same simulation frame window.

#### Scenario: PDO follows SYNC within budget
- **WHEN** a SYNC is scheduled for frame 5
- **THEN** the corresponding PDO frames are queued for transmission within 5 ms and observable in the same or next batch cycle

### Requirement: PDO consumer applies mapped values deterministically
When the stack receives a PDO destined for the local node, it SHALL apply the mapped data to object dictionary entries using deterministic simulation time, without side effects to remote node state.

#### Scenario: Sensor value reaches controller
- **WHEN** node 11 publishes a TPDO mapped to object `0x6000:01`
- **THEN** node 12 applies the received bytes to its matching RPDO mapping at the frame boundary

### Requirement: Heartbeat producer emits periodic boot-up and heartbeat frames
The stack SHALL emit boot-up NMT frame on initialization and heartbeat frames at the configured producer interval, incrementing a monotonic counter.

#### Scenario: Heartbeat interval is respected
- **WHEN** heartbeat producer time is configured to 1 s
- **THEN** heartbeat frames are published at roughly 1 s simulation intervals with increasing counters

### Requirement: Heartbeat and error states are observable as aggregate signals
The stack SHALL expose node heartbeat status and error event outputs as bounded POD that maps to `CanHeartbeat` and `CanAggregateErrors` for monitors.

#### Scenario: Missed heartbeats degrade status
- **WHEN** expected heartbeats from node 11 do not arrive for two consecutive periods
- **THEN** node 12 reports a degraded aggregate status flag without deadlocking

### Requirement: Two-node skeleton demonstrates end-to-end PDO path
The skeleton SHALL instantiate node 11 as sensor `sensor_a` and node 12 as controller `controller_b`, proving a deterministic PDO round trip over `CAN_A`.

#### Scenario: Repeated runs match
- **WHEN** the two-node scenario is run twice with the same seed and plan
- **THEN** every received PDO payload and every published response matches between runs

### Requirement: Protocol processing is allocation-free after arming
After arming, CANopen state transitions, PDO mapping, and filter evaluation SHALL use only pre-allocated storage.

#### Scenario: Step with active PDO traffic is allocation-free
- **WHEN** the component is armed and SYNC/PDO traffic is active
- **THEN** per-step allocation instrumentation reports zero heap allocations
