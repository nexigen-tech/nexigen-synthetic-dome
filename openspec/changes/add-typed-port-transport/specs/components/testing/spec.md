# Spec Delta

## ADDED Requirements

### Requirement: Headless harness supports typed port fixtures
The test harness SHALL allow tests to supply typed input port data and capture typed output port data without launching processes or using a real transport.

#### Scenario: Test feeds a RawCanBatch to a component input port
- **WHEN** a test declares input fixture data for a component's `in` port
- **THEN** the component's step callback receives that data as a typed view

#### Scenario: Test reads component output port data
- **WHEN** a test declares an output capture for a component's `out` port
- **THEN** the harness returns the written samples after each step

### Requirement: Headless harness validates port fixture layout
The harness SHALL reject fixture data whose size or `layout_hash` does not match the declared port contract.

#### Scenario: Wrong fixture size is rejected
- **WHEN** a test provides a fixture buffer whose size differs from the contract sample size
- **THEN** the harness reports a layout mismatch before stepping

### Requirement: Headless harness supports deterministic port replay
The harness SHALL replay the same input sequence across multiple runs and compare output sequences byte-for-byte for deterministic components.

#### Scenario: Repeated port replay matches
- **WHEN** a deterministic component is stepped twice with the same input fixtures
- **THEN** both output captures are identical
