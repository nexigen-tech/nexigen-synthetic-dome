# Spec Delta

## Purpose

Provides a headless verification environment that proves component lifecycle, deterministic behavior, ABI compatibility, and hot-path resource rules without requiring a running simulation system.

## ADDED Requirements

### Requirement: Frame-driven headless execution

The test harness SHALL initialize, arm, step, and stop components with caller-provided frame contexts and typed inputs without iceoryx2, Chronos, runner, Qt, Tauri, or a real-time thread.

#### Scenario: Component test supplies two frames

- **WHEN** a test supplies inputs for frames N and N+1 through the headless harness
- **THEN** the component observes those frames in order and the harness captures the corresponding typed outputs

### Requirement: Deterministic repeated execution

The harness SHALL support repeating a frame sequence with identical configuration, initial state, and seed and comparing outputs or state hashes per frame.

#### Scenario: Repeated sequence has identical results

- **WHEN** the same deterministic component executes the same input sequence twice
- **THEN** every compared output or state hash matches, and any mismatch identifies the first differing frame and component

### Requirement: C++ allocation-after-arm detection

The C++ test harness SHALL detect heap allocation attempted after successful arming and report the offending test without enabling production hot-path instrumentation.

#### Scenario: C++ component allocates in step

- **WHEN** a test C++ component allocates after arming
- **THEN** the harness records the allocation and fails the test while a non-allocating component passes

### Requirement: Rust allocation-after-arm detection

The Rust test harness SHALL use a test allocator or equivalent test-only mechanism to detect allocation after arming.

#### Scenario: Rust component allocates in step

- **WHEN** a test Rust component allocates after arming
- **THEN** the test allocator records the violation and the test fails while a non-allocating Rust component passes

### Requirement: Cross-language lifecycle equivalence

The harness SHALL execute equivalent C++ and Rust components through the same lifecycle and compare their behavior using the same contracts.

#### Scenario: C++ and Rust components produce matching outputs

- **WHEN** equivalent C++ and Rust test components receive identical configuration and frame inputs
- **THEN** their lifecycle transitions and output bytes match for every tested frame

### Requirement: Rust panic tests use process isolation

Panic/FFI verification SHALL execute in a child process so an abort cannot terminate the main test runner or be mistaken for a caught language exception.

#### Scenario: Panic does not unwind into the host

- **WHEN** the child host invokes a Rust component that panics
- **THEN** the child terminates abnormally and the parent test verifies that execution did not continue into another component frame

### Requirement: Headless performance reporting

The harness SHALL measure empty-component step overhead separately from component work and SHALL NOT present it as transport, scheduler, or end-to-end real-time latency.

#### Scenario: ABI overhead is reported

- **WHEN** the empty-component benchmark completes
- **THEN** the report identifies the host/build configuration and measured call overhead without claiming an iceoryx2 or simulation-cycle latency

### Requirement: Verification covers supported build modes

The change SHALL provide C++ Debug, Release, ThreadSanitizer, AddressSanitizer/UBSan, and Clang coverage, plus Rust debug/release tests and lint checks.

#### Scenario: Verification matrix completes

- **WHEN** the documented build and test matrix is executed on a supported development host
- **THEN** all correctness and sanitizer tests pass, and any strict timing check is reported separately with its host qualification
