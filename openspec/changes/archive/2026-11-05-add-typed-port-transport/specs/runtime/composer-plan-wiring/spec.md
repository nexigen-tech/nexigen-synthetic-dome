# Spec Delta

## Purpose

Extends the Composer to validate and emit explicit port wiring in the Dome Plan, ensuring every required port is bound to a compatible logical source.

## ADDED Requirements

### Requirement: Composer validates port wiring in the Dome Plan
The Composer SHALL ensure that every required port is connected to exactly one logical source, that the source and sink contracts are compatible, and that no port is left unbound or doubly bound.

#### Scenario: Required input port is unbound
- **WHEN** a component instance declares a required input port but the plan has no wire for it
- **THEN** Composer validation fails naming the instance and port

#### Scenario: Output port bound to two different source names
- **WHEN** a plan wires the same output port to both `CAN_A` and `CAN_B`
- **THEN** Composer validation fails with a duplicate-binding diagnostic

### Requirement: Composer emits explicit Wire entries in the Dome Plan
The Dome Plan SHALL contain a `Wire` entry for every port binding, recording source instance/port, target logical source name, and contract reference.

#### Scenario: Plan contains a CAN_A wire
- **WHEN** Composer validates a system with `virtual_can.out` bound to `CAN_A` and `can_monitor.in` bound to `CAN_A`
- **THEN** the emitted Dome Plan contains a `Wire` entry for source name `CAN_A` referencing both ports

### Requirement: Composer validates source-name uniqueness per output port
The Composer SHALL reject a plan where two output ports publish to the same logical source name with incompatible contracts.

#### Scenario: Two publishers with different contract hashes on same source
- **WHEN** two output ports write to `CAN_A` but their contract `layout_hash` values differ
- **THEN** Composer validation fails before the plan is emitted
