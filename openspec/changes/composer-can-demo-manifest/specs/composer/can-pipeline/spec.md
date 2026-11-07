# Spec Delta

## Purpose

Defines the Composer-based CAN pipeline demonstration: a repeatable workflow that takes a System Manifest describing CAN source and sink instances, validates it through the Composer, emits a Dome Plan, and executes it through `nexigen-run`.

## ADDED Requirements

### Requirement: Manifest declares CAN source and sink as component instances
The System Manifest SHALL declare a `CanSource` instance with one output port `"out"` bound to contract `RawCanBatchClassical` and a `CanSink` instance with one input port `"in"` bound to the same contract. Instances SHALL belong to the same partition.

#### Scenario: Manifest parses successfully
- **WHEN** the manifest JSON is loaded by `SystemManifest::from_json()`
- **THEN** it returns a valid `SystemManifest` with 2 instances, 1 partition, 1 wire

### Requirement: Composer validates intra-partition wiring
The Composer SHALL validate that the CAN source and sink share the same partition and that their wire requires no LET boundary. Validation SHALL pass with zero diagnostics.

#### Scenario: Intra-partition wire passes validation
- **WHEN** `validate_intra_partition_direct_wiring()` is called with the CAN pipeline Dome Plan
- **THEN** the function returns an empty diagnostics vector

### Requirement: Composer emits a Dome Plan from the manifest
The Composer's `emit_plan()` SHALL transform the validated System Manifest + loaded descriptors into a concrete `DomePlan` that references the correct executables, wires, and Chronos configuration.

#### Scenario: Emitted plan contains expected structure
- **WHEN** `emit_plan()` is called with the CAN pipeline manifest and descriptors
- **THEN** the returned `DomePlan` has exactly 1 partition, 1 wire, and 2 processes
- **THEN** the wire connects `"can_source"::"out"` → `"can_sink"::"in"`

### Requirement: nexigen-run executes the emitted Dome Plan
The `nexigen-run` SHALL accept the emitted Dome Plan, spawn the CAN source and sink processes, drive one Chronos frame, and shut down cleanly.

#### Scenario: Single frame executes successfully
- **WHEN** `nexigen-run` is invoked with the CAN pipeline Dome Plan
- **THEN** it reports "completed frame 1" and exits with code 0

### Requirement: Demo is self-documenting
The demo directory SHALL contain a README with the full workflow: manifest authoring, descriptor generation, validation, plan emission, and execution commands.

#### Scenario: README covers all steps
- **WHEN** a developer follows the README from step 1 to step N
- **THEN** the pipeline executes without errors