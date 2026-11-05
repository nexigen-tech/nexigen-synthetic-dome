# Spec Delta

## Purpose

Defines how Logical Execution Time boundaries apply to cross-partition port wiring and how LET metadata is carried on transported samples.

## ADDED Requirements

### Requirement: Cross-partition port writes are declared LET boundaries
The Composer SHALL mark every wire whose source and sink instances belong to different partitions as a LET boundary and record the read time and write deadline.

#### Scenario: Sensor and monitor in different partitions
- **WHEN** `virtual_can` is in partition P1 and `can_monitor` is in partition P2 and they share a wire
- **THEN** the Dome Plan declares a LET boundary for that wire with a read time and write deadline

### Requirement: LET boundary metadata is carried on transported samples
Samples crossing a LET boundary SHALL carry the declared read time and write deadline so the runtime can detect deadline violations.

#### Scenario: Late write is recorded
- **WHEN** a producer writes a sample after the declared write deadline
- **THEN** the runtime records a deadline violation for that frame without blocking the consumer

### Requirement: Intra-partition port writes are not LET boundaries
Wires whose source and sink instances belong to the same partition SHALL NOT be marked as LET boundaries and SHALL be routed through the fastest available transport path.

#### Scenario: Same-partition virtual source and CANopen node
- **WHEN** `virtual_can` and `controller_b` share partition P1
- **THEN** their wire is routed directly without LET read-time/deadline metadata
