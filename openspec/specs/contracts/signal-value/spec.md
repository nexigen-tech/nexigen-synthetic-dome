# contracts/signal-value Specification

## Purpose
Provides a typed scalar sample contract that carries a value together with per-sample quality and origin metadata so that logical ports can transport individual measurements from models, operators, files, generators, or replay.

## Requirements

### Requirement: SignalValue carries value, quality, and origin
The `SignalValue<T>` aggregate SHALL contain exactly one typed `value`, one `Quality` indicator, and one `Origin` indicator.

#### Scenario: Manual override changes origin
- **WHEN** an operator overrides a model-produced `SignalValueF32`
- **THEN** the overridden sample reports `origin = Manual` while a model-produced sample on the same port reports `origin = Model`

### Requirement: Quality covers valid, stale, error, and unavailable states
The `Quality` enum SHALL provide distinct values for `Valid`, `Stale`, `E2EError`, and `NotAvailable`.

#### Scenario: Stale value is distinguishable from valid
- **WHEN** a producer marks a sample as `Stale`
- **THEN** consumers read `quality = Stale` and do not confuse it with `Valid`

### Requirement: Concrete instantiations cover common numeric types
`nxdc` SHALL emit concrete contracts for `f32`, `f64`, `i32`, `i64`, `u32`, and `u64` from a single parameterized `SignalValue` schema.

#### Scenario: Each concrete type has identical layout semantics
- **WHEN** `SignalValueF32` and `SignalValueU64` are generated
- **THEN** both have the same field order, the same `Quality`/`Origin` offsets, and a non-zero `layout_hash`

### Requirement: Generated SignalValue is zero-copy safe
The generated C++ and Rust types SHALL be trivially copyable, contain no pointers, and satisfy the project zero-copy transport contract.

#### Scenario: Rust type implements ZeroCopySend
- **WHEN** `nxdc emit-rust` generates `SignalValueF32`
- **THEN** the emitted module contains a `const` assertion that the type satisfies the `ZeroCopySend` contract
