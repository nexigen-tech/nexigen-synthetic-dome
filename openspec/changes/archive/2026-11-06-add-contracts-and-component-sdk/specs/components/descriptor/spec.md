# Spec Delta

## ADDED Requirements

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
