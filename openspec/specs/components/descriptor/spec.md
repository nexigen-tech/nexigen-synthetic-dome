# Component Descriptor Specification

## Purpose

Defines the reusable, declarative identity and requirements of a component so hosts and later composition tools can validate it without knowing its implementation language or physical deployment.

## Requirements

### Requirement: Component and SDK identity

A component descriptor SHALL declare a stable component name, semantic component version, plugin SDK ABI major/minor, and all contract identities with their required versions and hashes.

#### Scenario: Incompatible SDK is detected

- **WHEN** a component requires an SDK ABI major different from the host
- **THEN** descriptor validation fails before the plugin is loaded or initialized

### Requirement: Logical port declarations

A descriptor SHALL declare each port's logical role, direction, semantics (`Signal`, `Event`, `Stream`, or `Command`), contract, and whether a binding is required, without physical channel or CPU information.

#### Scenario: CANopen component remains hardware-independent

- **WHEN** a component declares a raw CAN port
- **THEN** it names only the logical port role and raw CAN contract, not a PCIe module, physical channel, or transport endpoint

### Requirement: Parameter schemas and file inputs

A descriptor SHALL define typed parameters, defaults, allowed ranges, and file-valued parameters used during initialization.

#### Scenario: XDD file is declared as initialization input

- **WHEN** a CANopen component descriptor declares an object-dictionary file parameter
- **THEN** the host can validate and load that file during initialization without exposing file parsing to frame stepping

#### Scenario: Invalid parameter is rejected

- **WHEN** a supplied parameter has the wrong type, lies outside its allowed range, or omits a required file
- **THEN** initialization fails with a diagnostic naming the parameter and no armed component remains

### Requirement: Declared thread roles

A descriptor SHALL declare its configurable thread roles, whether each role is required, whether it requires one CPU, and its default scheduling requirements.

#### Scenario: Gateway roles are validated

- **WHEN** a gateway descriptor declares required single-CPU RX and TX roles
- **THEN** the host accepts matching thread specifications and rejects missing roles or multi-CPU assignments before arming

### Requirement: Resource and determinism declarations

A descriptor SHALL declare bounded memory and execution budgets and flags for real-time safety, determinism, RNG use, and hardware real-time requirements.

#### Scenario: Nondeterministic component is visible to tooling

- **WHEN** a component declares `deterministic: false`
- **THEN** validation and recording tools can report that deterministic replay cannot be claimed from configuration and seed alone

### Requirement: Supported execution profiles and modes

A descriptor SHALL declare which execution profiles and time modes it supports, including whether it requires real-time hardware operation.

#### Scenario: Hardware component cannot enter virtual-time mode

- **WHEN** a component requiring real-time hardware is configured for Scaled, Free, or Step mode
- **THEN** validation rejects the configuration before component initialization

### Requirement: Descriptor parsing is all-or-nothing

Descriptor loading SHALL reject malformed or inconsistent input without leaving a partially initialized component definition.

#### Scenario: Unknown contract or contradictory thread role

- **WHEN** a descriptor references an unknown contract or assigns contradictory requirements to one thread role
- **THEN** loading fails with all diagnostics available to the caller and no partial descriptor is accepted

### Requirement: Port contract references include version and layout hash
Each port declaration SHALL reference a contract by name, required major version, and `layout_hash` so the host can validate plugin compatibility before arming.

#### Scenario: Descriptor declares contract version for a port
- **WHEN** a descriptor declares an output port on `RawCanBatchClassical` version `1.0.0`
- **THEN** the emitted descriptor JSON contains the contract name, major version, and 32-byte `layout_hash`

### Requirement: Component SDK identity exposes ABI version and stable name
The descriptor SHALL declare the plugin SDK ABI major/minor version and a stable component type name separate from the shared library file name.

#### Scenario: Incompatible SDK ABI is rejected before loading
- **WHEN** a descriptor requires SDK ABI major `2` and the host supports ABI major `1`
- **THEN** descriptor validation fails before the plugin shared library is loaded
