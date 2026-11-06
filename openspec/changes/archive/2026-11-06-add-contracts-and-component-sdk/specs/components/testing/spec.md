# Spec Delta

## ADDED Requirements

### Requirement: Golden cross-language binary vectors verify contract bytes
The test suite SHALL provide golden byte fixtures that prove C++ and Rust produce and consume identical binary representations for representative contracts.

#### Scenario: C++ writes and Rust reads the same bytes
- **WHEN** a C++ test writes a golden `SampleHeader` and `SignalValueF32` to a shared memory buffer
- **THEN** a Rust test reads the same buffer and observes identical field values without conversion

### Requirement: Simple C++ and Rust components pass identical headless lifecycle
The headless harness SHALL drive equivalent C++ and Rust components through `create`, `initialize`, `arm`, repeated `step`, `stop`, and `destroy` and compare their behavior.

#### Scenario: Counter components match byte-for-byte
- **WHEN** a C++ counter plugin and a Rust counter plugin receive the same seed and configuration
- **THEN** their step outputs are identical for every tested frame

### Requirement: Allocation-after-arm detection covers C++ and Rust
The test harness SHALL detect heap allocations attempted after successful arming for both C++ plugins and Rust plugins without enabling production instrumentation.

#### Scenario: Rust allocating component fails the test
- **WHEN** a Rust test plugin allocates during `step`
- **THEN** the test allocator records the violation and the harness test fails
