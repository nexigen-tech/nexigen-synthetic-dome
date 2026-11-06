# Spec Delta

## ADDED Requirements

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
