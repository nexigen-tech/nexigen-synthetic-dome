# Spec Delta

## Purpose

Provides a fixed-size common metadata header that accompanies every transported sample so that frame identity, timing, source identity, and sequence information cross the data plane without extra allocations.

## ADDED Requirements

### Requirement: SampleHeader contains fixed-size metadata
The `SampleHeader` contract SHALL contain `frame_no`, `sim_time_ns`, `hw_time_ns`, `source_id`, `sequence`, and `flags` fields.

#### Scenario: Header size is known at compile time
- **WHEN** `nxdc emit-cpp` generates `SampleHeader.hpp`
- **THEN** the generated header static-asserts `sizeof(SampleHeader)` equals the documented fixed size

### Requirement: SampleHeader identifies source and ordering
The `source_id` and `sequence` fields SHALL uniquely identify the logical source and the monotonic sample order for that source.

#### Scenario: Two samples from the same source are ordered
- **WHEN** a source emits two samples
- **THEN** the second sample has a greater `sequence` than the first and both share the same `source_id`

### Requirement: SampleHeader maps to the transport user header
The runtime transport layer SHALL use `SampleHeader` as the iceoryx2 user header for every published sample.

#### Scenario: Publisher writes header and subscriber reads it back
- **WHEN** a C++ publisher fills `SampleHeader` and publishes a sample
- **THEN** a Rust subscriber receives the same `frame_no`, `sim_time_ns`, `source_id`, and `sequence` in the user header

### Requirement: SampleHeader is generated for C++ and Rust
`nxdc` SHALL emit `SampleHeader` as a C++23 POD header and a Rust `#[repr(C)]` module with matching size, alignment, and field offsets.

#### Scenario: Cross-language layout assertions pass
- **WHEN** the C++ and Rust generated types are compiled
- **THEN** both enforce identical `size_of`, `align_of`, and `offset_of` values for every field
