# Contract Schema Specification

## Purpose

Provides one declarative source for versioned, bounded, cross-language data contracts so independently built C++ and Rust components can exchange identical binary values safely.

## Requirements

### Requirement: Declarative typed contract aggregates

The system SHALL define contracts as versioned typed aggregates on logical ports and SHALL support nested records, fixed-width scalars, enums, and fixed-size arrays.

#### Scenario: Aggregate contract is generated

- **WHEN** a schema declares several semantic fields for one logical output port
- **THEN** the generated contract exposes one typed aggregate containing those fields rather than requiring one transport service per scalar

### Requirement: Common sample metadata

Every transported sample SHALL have fixed-size metadata containing frame number, simulation time, hardware time, numeric source identity, sequence number, and flags.

#### Scenario: Metadata identifies source and ordering

- **WHEN** two samples from the same source are inspected
- **THEN** their source identity is unchanged and their sequence numbers increase monotonically, including when one sample is a heartbeat

### Requirement: Semantic field quality and origin

Each semantic signal field SHALL carry its value, quality (`Valid`, `Stale`, `E2EError`, or `NotAvailable`), and origin (`Model`, `Manual`, `File`, `Generator`, or `Replay`).

#### Scenario: Fields in one aggregate have different origins

- **WHEN** an operator overrides one field while the model continues producing another field in the same aggregate
- **THEN** the overridden field reports `Manual` and the model-produced field retains `Model` with independent quality values

### Requirement: Shared-memory-compatible bounded values

Transport contracts SHALL have a fixed layout, fixed maximum storage, and no pointers, owning heap references, runtime-sized strings, or non-trivial destruction.

#### Scenario: Unbounded field is rejected

- **WHEN** a schema contains a runtime-sized string, vector, pointer, or unbounded payload
- **THEN** schema generation fails with a diagnostic identifying the unsupported field before any binary is produced

### Requirement: Equivalent C++ and Rust layouts

Each schema SHALL generate C++ and Rust representations with identical field offsets, sizes, alignments, numeric representations, and stable transport type identities.

#### Scenario: Cross-language layout assertions succeed

- **WHEN** generated C++ and Rust types for the same schema are compared
- **THEN** all size, alignment, and field-offset assertions pass and their transport type identities match

#### Scenario: Generated values are shared-memory safe

- **WHEN** generated Rust types are checked for transport use
- **THEN** they have a C-compatible representation, satisfy the required zero-copy type contract, and contain no invalid Rust enum or pointer representation

### Requirement: Deterministic compatibility hash

Each contract SHALL have a deterministic `layout_hash` derived from its canonical layout and wire semantics. Contract consumers SHALL require matching contract names, major versions, and hashes.

#### Scenario: Incompatible consumers are rejected

- **WHEN** two endpoints use the same contract name but different major versions or `layout_hash` values
- **THEN** compatibility validation fails before component initialization

#### Scenario: Documentation-only edit preserves compatibility

- **WHEN** only a field description or display label changes without affecting layout or wire semantics
- **THEN** the canonical `layout_hash` remains unchanged

### Requirement: Schema metadata for cold-path tooling

A schema SHALL support optional units, ranges, enum labels, defaults, and display metadata, and SHALL emit machine-readable field descriptors for cold-path tools.

#### Scenario: Tool reads field metadata without C++ or Rust types

- **WHEN** a panel or diagnostic tool loads a generated field descriptor
- **THEN** it can identify the field name, primitive type, offset, unit, range, and enum labels without linking the generated type library

### Requirement: Cold-path diagnostics never enter frame stepping

Generated public contract values SHALL support cold-path diagnostic and JSON representations without performing serialization or formatting during frame stepping.

#### Scenario: Hot path avoids serialization

- **WHEN** a component steps through a frame after arming
- **THEN** no schema parser, JSON serializer, string formatter, or heap allocation is invoked by the generated transport types

### Requirement: Golden cross-language binary vectors

The system SHALL provide golden binary fixtures proving that C++ and Rust produce and consume identical bytes for representative contracts.

#### Scenario: C++ and Rust agree in both directions

- **WHEN** C++ writes a golden value for Rust to read and Rust writes the corresponding value for C++ to read
- **THEN** both readers observe identical field values and byte sequences with no conversion or re-serialization

### Requirement: Contract names are stable identifiers
The contract `name` field SHALL be a stable identifier that is independent of the source file path and determines the generated C++ namespace and Rust module name.

#### Scenario: Renaming the source file does not change the contract name
- **WHEN** a contract YAML file is moved or renamed without changing its `name` field
- **THEN** generated bindings, `layout_hash`, and compatibility checks remain unchanged

### Requirement: Contract version uses semantic versioning
Each contract SHALL declare a `version` string in `major.minor.patch` form. Only the major version SHALL participate in compatibility checks together with the `layout_hash`.

#### Scenario: Major version mismatch is rejected
- **WHEN** a consumer references `SampleHeader` version `2.0.0` while the producer declares version `1.0.0`
- **THEN** compatibility validation fails before any sample is exchanged

### Requirement: SampleHeader is the canonical metadata contract
The contract schema tooling SHALL treat `SampleHeader` as the canonical per-sample metadata contract and SHALL generate it as a first-class contract with its own `layout_hash`.

#### Scenario: Transport layer references SampleHeader layout hash
- **WHEN** the runtime transport creates a publisher
- **THEN** it can request the `layout_hash` of the `SampleHeader` contract from the generated contract catalog
