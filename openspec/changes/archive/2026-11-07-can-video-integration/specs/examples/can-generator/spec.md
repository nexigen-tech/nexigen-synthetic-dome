# Spec Delta

## Purpose

Provides a standalone Rust console application that generates deterministic CAN messages on a named iceoryx2 bus at 1 ms intervals, serving as a reference for Rust SDK + transport integration for external developers.

## ADDED Requirements

### Requirement: CAN generator publishes one batch per millisecond
The CAN generator SHALL connect to an iceoryx2 CAN bus by logical source name and publish one `RawCanBatchClassical` sample every 1 ms on that bus, with frame content that varies each tick to simulate realistic traffic.

#### Scenario: Generator starts and publishes on named bus
- **WHEN** the generator starts with `--can-bus "CAN_DEMO"`
- **THEN** it connects to the iceoryx2 service `nxd/CAN_DEMO`
- **THEN** it begins publishing `RawCanBatchClassical` samples every 1 ms

#### Scenario: Generator publishes frame with varying content
- **WHEN** the generator has been running for 5 ms
- **THEN** frames published at tick 0 and tick 4 SHALL have different payload content

### Requirement: Bus name is configurable via CLI argument
The generator SHALL accept a `--can-bus <name>` CLI argument that selects the iceoryx2 logical source name to publish on. When the argument is omitted, a default name (`CAN_DEMO`) SHALL be used.

#### Scenario: Generator uses custom bus name
- **WHEN** the generator is started with `--can-bus "DEMO_BUS_1"`
- **THEN** it connects to iceoryx2 service `nxd/DEMO_BUS_1`

#### Scenario: Generator uses default bus name
- **WHEN** the generator is started without `--can-bus`
- **THEN** it connects to iceoryx2 service `nxd/CAN_DEMO`

### Requirement: Generator exits cleanly on SIGINT
The generator SHALL catch SIGINT (Ctrl+C), stop publishing, perform an orderly shutdown of the iceoryx2 transport, and exit with code 0.

#### Scenario: SIGINT triggers graceful shutdown
- **WHEN** the generator is running and receives SIGINT
- **THEN** it stops publishing new batches within one frame interval
- **THEN** it exits with exit code 0

### Requirement: Generator produces no heap allocation after initial setup
After the iceoryx2 transport and publisher are created during initialization, the generator SHALL not allocate heap memory, acquire locks, or spawn threads while producing batches in the publish loop.

#### Scenario: Producer loop is allocation-free
- **WHEN** the generator has been running for 1 second
- **THEN** no heap allocations occurred after the first frame
