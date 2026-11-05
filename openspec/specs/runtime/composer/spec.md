# runtime/composer Specification

## Purpose
Provides a declarative composition layer that transforms a system manifest into a validated Dome Plan describing processes, partitions, LET boundaries, thread roles, and channel wiring, and that can drive a minimal local execution via `nexigen-run`.

The Composer does not execute the simulation itself. It validates intent, resolves references, checks compatibility, and produces an executable plan consumed by the runtime and tooling.

## Requirements

### Requirement: Declarative system manifest

The Composer SHALL accept a system manifest that declares components, their instances, partitions, execution mode, base frame interval, channel wiring, parameter assignments, and thread-role bindings.

#### Scenario: Manifest declares two partitions

- **WHEN** a manifest groups components into two partitions with a LET boundary between them
- **THEN** the Composer preserves the partition structure in the Dome Plan

### Requirement: Validation against component descriptors

The Composer SHALL validate every component instance against its descriptor, including required ports, parameters, thread roles, supported modes, and contract identities with matching `layout_hash` values.

#### Scenario: Missing required port is rejected

- **WHEN** a manifest omits a binding for a descriptor-declared required port
- **THEN** validation fails with a diagnostic naming the component instance and port

### Requirement: Contract compatibility validation

The Composer SHALL verify that connected ports use compatible contracts by name, major version, and `layout_hash`.

#### Scenario: Incompatible contracts on a wire

- **WHEN** two connected ports use contracts with different `layout_hash` values
- **THEN** validation fails before a Dome Plan is produced

### Requirement: Thread role and scheduling validation

The Composer SHALL validate that every declared thread role is assigned to exactly one execution thread, that required roles are present, and that scheduling/affinity declarations are internally consistent.

#### Scenario: Required role is unassigned

- **WHEN** a component descriptor declares a required thread role and the manifest does not assign it
- **THEN** validation fails naming the missing role and component instance

### Requirement: Chronos policy validation

The Composer SHALL validate the selected execution mode, base frame interval, and hardware-time requirements against the supported modes declared by every component descriptor.

#### Scenario: Hardware-only component in Free SIL

- **WHEN** a component requiring RT/HIL hardware is placed in a Free SIL system
- **THEN** validation rejects the configuration with a mode-incompatibility diagnostic

### Requirement: LET boundary validation

The Composer SHALL enforce that Direct communication is declared only inside a partition and that cross-partition traffic crosses an explicit LET boundary.

#### Scenario: Cross-partition Direct wiring rejected

- **WHEN** a manifest wires two ports in different partitions without a LET boundary
- **THEN** validation fails naming the ports and partitions

### Requirement: Dome Plan output

The Composer SHALL produce a Dome Plan containing resolved processes, partition membership, LET boundaries, channel wiring with transport hints, thread-role assignments, parameter values, and Chronos configuration.

#### Scenario: Plan is consumed by nexigen-run

- **WHEN** `nexigen-run` loads a Dome Plan
- **THEN** it can launch the described processes, configure partitions, and start Chronos without re-parsing the original manifest

### Requirement: Local multi-process deployment intent

The Dome Plan SHALL describe deployment intent for local multi-process execution on Ubuntu 26.04, including process grouping, shared-memory blackboard configuration, and executable paths.

#### Scenario: Single-node plan describes two processes

- **WHEN** a plan groups components into two processes
- **THEN** `nexigen-run` launches two OS processes and connects them through the configured iceoryx2 blackboard

### Requirement: nexigen-run walking skeleton

`nexigen-run` SHALL be able to load a Dome Plan, validate it, launch the local process graph, start Chronos, run at least one deterministic frame, and shut down cleanly on completion or operator stop.

#### Scenario: Walking skeleton runs one frame

- **WHEN** `nexigen-run` is invoked with the `nxd-ws01` plan
- **THEN** it initializes Chronos, launches the configured processes, distributes one frame context, and stops without leaks or hung processes

### Requirement: Validation diagnostics

Validation failures SHALL report file, line, component instance, port, contract, role, or mode information needed to locate and fix the manifest.

#### Scenario: Invalid parameter reports location

- **WHEN** a parameter value violates its descriptor-declared range
- **THEN** the diagnostic names the manifest file, component instance, parameter, and allowed range

### Requirement: Composer performs no runtime logic

The Composer SHALL not implement frame scheduling, time distribution, or component execution. Those responsibilities belong to Chronos and the host runtime.

#### Scenario: Composer output is consumed by runtime

- **WHEN** a Dome Plan is produced
- **THEN** it contains only declarative configuration and intent; no scheduler state or component code is embedded
