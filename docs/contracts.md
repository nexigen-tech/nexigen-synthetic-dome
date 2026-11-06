# NXD Contract IDL

This document describes the declarative contract IDL parsed by the `nxdc`
schema compiler, the deterministic canonicalization rules used to produce the
`layout_hash`, and the `nxdc` CLI commands that operate on contracts.

## Overview

A contract is a versioned, bounded, language-neutral aggregate.  `nxdc` reads
one YAML file per contract, validates it, and emits:

- a canonical JSON document;
- a 32-byte SHA-256 `layout_hash` of that document;
- C++23 POD headers with compile-time size/alignment/offset assertions;
- Rust `#[repr(C)]` sources with `core::mem` layout assertions;
- a machine-readable field descriptor JSON.

The hot-path generated types contain no parser, formatter, allocator, or
locking.  All layout decisions are made by `nxdc` on the cold path and encoded
in the canonical document.

Skeleton contracts include `SampleHeader` (common per-sample metadata),
`SignalValue<T>` (typed scalar with quality and origin), `RawCanFrame`,
`RawCanBatch`, `VideoFrame`, and their heartbeat/error aggregates.

## IDL vocabulary

A contract file is a single YAML document with a top-level `contract` mapping,
an optional `types` list of user-defined records and enums, and a `fields`
list that defines the top-level aggregate.

### Contract header

```yaml
contract:
  name: SampleHeader
  version: "1.0.0"
  description: Common sample metadata for every transported sample.
```

- `name` – stable contract identifier; must be a valid C-style identifier.
- `version` – semantic `major.minor.patch` version.  Only the major version
  participates in compatibility checks together with the `layout_hash`.
- `description` – human-readable text; excluded from the `layout_hash`.

### Primitives

Wire types are fixed-width scalars only.  The following type names are
supported:

| Type  | Size | Alignment |
|-------|------|-----------|
| `i8`  | 1    | 1         |
| `i16` | 2    | 2         |
| `i32` | 4    | 4         |
| `i64` | 8    | 8         |
| `u8`  | 1    | 1         |
| `u16` | 2    | 2         |
| `u32` | 4    | 4         |
| `u64` | 8    | 8         |
| `f32` | 4    | 4         |
| `f64` | 8    | 8         |

`bool`, `long`, `size_t`, `isize`, `usize`, pointers, references, strings,
slices, `Vec`, `Box`, and generic containers are rejected as unbounded or
non-portable.

Example (`tools/nxdc/tests/fixtures/valid/sample_header.yaml`):

```yaml
fields:
  - name: frame_no
    type: u64
  - name: sim_time_ns
    type: i64
  - name: source_id
    type: u32
```

### Fixed arrays

Arrays use the syntax `T[N]` where `T` is a primitive or named type and `N` is
a positive decimal integer.

```yaml
fields:
  - name: history
    type: f32[8]
```

The array size is `sizeof(T) * N` and its alignment equals `alignof(T)`.
Zero-length or non-numeric counts are rejected.

### Records

A record groups fields under a named type.  Records are declared in the
`types` section and referenced by name.  Field order in the source is preserved
because it determines offsets and padding.

```yaml
types:
  - name: SignalF32
    kind: record
    fields:
      - name: value
        type: f32
      - name: quality
        type: Quality
      - name: origin
        type: Origin

fields:
  - name: signal
    type: SignalF32
```

See `tools/nxdc/tests/fixtures/valid/nested_record.yaml` for a complete
example.

### Template parameters and concrete instantiation

A contract may declare template parameters for array capacities or field
types.  Integer parameters select fixed array capacities; type parameters
select from an allow-list of primitive types.

```yaml
contract:
  name: RawCanBatch
  version: "1.0.0"

params:
  - name: max_payload_bytes
    type: u32
    min: 8
    max: 2048
    default: 64
  - name: batch_capacity
    type: u32
    default: 32

fields:
  - name: frames
    type: CanFrame[max_payload_bytes]
  - name: count
    type: u32
```

A template contract must provide one or more `concrete` instantiations.  `nxdc`
substitutes parameter values, validates each concrete schema independently, and
emits a separate binding for each instance.

```yaml
concrete:
  - name: RawCanBatchClassical
    params:
      max_payload_bytes: 8
      batch_capacity: 16
  - name: RawCanBatchFd
    params:
      max_payload_bytes: 64
      batch_capacity: 32
```

A type parameter restricts the concrete type to a fixed allow-list.  The
following declares `SignalValue<T>` with per-field `Quality` and `Origin`, then
emits concrete bindings for each allowed primitive:

```yaml
contract:
  name: SignalValue
  version: "1.0.0"

params:
  - name: T
    kind: type
    allowed: [f32, f64, i32, i64, u32, u64]
    default: f32

types:
  - name: Quality
    kind: enum
    underlying: u8
    values:
      - name: Valid
        value: 0
      - name: Stale
        value: 1
      - name: E2EError
        value: 2
      - name: NotAvailable
        value: 3

  - name: Origin
    kind: enum
    underlying: u8
    values:
      - name: Model
        value: 0
      - name: Manual
        value: 1
      - name: File
        value: 2
      - name: Generator
        value: 3
      - name: Replay
        value: 4

fields:
  - name: value
    type: T
  - name: quality
    type: Quality
  - name: origin
    type: Origin

concrete:
  - name: SignalValueF32
    params:
      T: f32
  - name: SignalValueF64
    params:
      T: f64
```

Duplicate instance names, missing required parameters, out-of-range integer
values, and disallowed concrete types are rejected before emission.  See
`contracts/SignalValue.yaml`, `contracts/RawCanBatch.yaml`, and
`tools/nxdc/tests/codegen.rs`.

### Enums

Enums are integer newtypes with named constants.  The underlying type must be
one of the integer primitives.

```yaml
types:
  - name: Quality
    kind: enum
    underlying: u8
    values:
      - name: Valid
        value: 0
      - name: Stale
        value: 1
```

Enum values must be non-negative and must fit in the underlying type.  The
wire representation is exactly the underlying integer; unknown numeric values
received from a producer are preserved for diagnostics and rejected only by
consumer semantic validation.

See `tools/nxdc/tests/fixtures/valid/can_frame.yaml` for enum examples used in
a CAN contract.

### Field metadata

Fields may carry optional cold-path metadata:

- `unit` – physical unit; included in the canonical document because it affects
  interpretation of transported bytes.
- `min`, `max` – inclusive scalar range; included in the canonical document.
- `description` – human-readable text; excluded from the `layout_hash`.
- `default` – initialization value; excluded from the `layout_hash`.

Example (`tools/nxdc/tests/fixtures/valid/field_metadata.yaml`):

```yaml
fields:
  - name: temperature
    type: f32
    unit: "degC"
    min: -40
    max: 150
    description: Motor temperature
```

## Validation

`nxdc` performs all-or-nothing validation and reports field-specific
diagnostics.  Rejected cases include:

- unbounded types (`string`, `Vec<u8>`, `bool`, ...);
- malformed types (`f128`, `u32[zero]`, ...);
- duplicate field, type, or enum-value names within the same scope;
- invalid version strings;
- unknown type references;
- enum values that do not fit in the underlying type.

The fixtures under `tools/nxdc/tests/fixtures/invalid/` exercise each of these
cases, and `tools/nxdc/tests/parse_fixtures.rs` asserts that the resulting
diagnostics name the offending path.

## Canonicalization and `layout_hash`

The `layout_hash` is the SHA-256 digest of a canonical UTF-8 JSON document.
The document uses the format identifier `"nxd-layout-v1"` so that future layout
algorithms are explicitly distinguishable.

### Included in the canonical document

- `format`, `name`, `version` (major/minor/patch integers);
- top-level aggregate `size` and `alignment`;
- every user-defined type with its `size`, `alignment`, `kind`, and, for
  records, ordered fields;
- every field with `name`, resolved `type`, `offset`, `size`, `alignment`;
- enum underlying types and numeric values;
- field `unit`, `min`, `max` (when present), because they affect byte
  interpretation.

### Excluded from the canonical document

- `description` and `default`;
- source file paths and formatting;
- comments and YAML-specific structure.

### Ordering

Object keys are emitted in lexicographic order.  Arrays that represent source
order (top-level fields and record fields) retain that order.  The JSON
serializer never emits floating-point layout metadata.

### Layout computation

`nxdc` computes natural-alignment layouts:

1. Each field starts at the next offset that is a multiple of its alignment.
2. The aggregate size is the end of the last field rounded up to the
   aggregate alignment (the maximum field alignment).
3. Explicit padding bytes are implicit in `offset`/`size`; no hidden growth is
   introduced.

Example: for `SampleHeader` (`tools/nxdc/tests/fixtures/valid/sample_header.yaml`)
the canonical document reports `size: 40` and `alignment: 8` because the final
`u32` field requires four bytes of trailing padding to reach an 8-byte
boundary.

### Stability requirements

Any change to the canonical document changes the `layout_hash`, which requires
a contract major-version bump under the conservative compatibility rule.  The
tests in `tools/nxdc/tests/canonical.rs` verify:

- repeated runs produce byte-identical JSON and hashes;
- the hash is independent of the source file path;
- description edits do not change the hash;
- layout edits change the hash;
- wire-semantic edits (e.g. `unit`) change the hash.

## `nxdc` CLI

The compiler is a workspace binary built with `cargo build -p nxdc`.

### `validate-contract`

Load and validate a contract schema:

```bash
nxdc validate-contract contracts/sample_header.yaml
```

On success it prints the contract name, version, type count, and field count.
On failure it prints every field-specific diagnostic.

### `validate-component`

Load and validate a component descriptor:

```bash
nxdc validate-component components/can_gateway.yaml
```

### `canonicalize`

Emit the canonical JSON document and the `layout_hash`:

```bash
nxdc canonicalize contracts/sample_header.yaml sample_header.canonical.json
```

If the output path is omitted, the JSON is printed to stdout.  The hash is
always printed.

### `emit-component-descriptor`

Emit the canonical JSON document and SHA-256 descriptor hash for a component:

```bash
nxdc emit-component-descriptor components/empty.yaml empty.descriptor.json
```

The output includes the component identity, SDK ABI version, ports, parameters,
thread roles, resources, and supported modes.  Descriptions and source paths are
excluded from the hash.

### `emit-cpp`

Emit a C++23 header for a concrete contract:

```bash
nxdc emit-cpp contracts/SampleHeader.yaml SampleHeader.hpp
```

For a templated contract with multiple `concrete` instantiations, pass a
Directory:

```bash
nxdc emit-cpp contracts/RawCanBatch.yaml generated_cpp/include/nxd/contracts/
```

Generated headers live in `nxd::contracts`, use `std::array` for fixed arrays,
insert explicit padding fields, and include `static_assert`s for `sizeof`,
`alignof`, `offsetof`, and trivial copyability.

### `emit-rust`

Emit a Rust `#[repr(C)]` source module:

```bash
nxdc emit-rust contracts/SampleHeader.yaml SampleHeader.rs
```

The output uses integer newtypes for enums, fixed-length arrays for repeated
fields, explicit padding fields, and `const _: () = assert!(...)` compile-time
checks for `size_of`, `align_of`, and `offset_of`.

Every generated type implements a `ZeroCopySend` marker trait
(`Copy + Send + Sync + 'static` plus a safety contract that the type is made
only of primitives and fixed arrays) and a final `const _: ()` block verifies
that every emitted type satisfies it.

### `emit-descriptors`

Emit a machine-readable field descriptor JSON document:

```bash
nxdc emit-descriptors contracts/RawCanFrame.yaml RawCanFrame.descriptor.json
```

The descriptor contains field names, resolved types, offsets, sizes, alignments,
units, ranges, and enum labels.

### C++ build integration

The root `CMakeLists.txt` generates C++ headers from the YAML contracts under
`contracts/` at build time and compiles one validation translation unit per
header.  This proves that all layout assertions hold under the project's
`-fno-exceptions` and `-fno-rtti` constraints.

## Test coverage

Every construct and example in this document is exercised by an existing test:

| Document section          | Fixture / test file                                    |
|---------------------------|--------------------------------------------------------|
| Contract header           | `valid/sample_header.yaml`, `contracts/SampleHeader.yaml` |
| Primitives                | `valid/sample_header.yaml`, `contracts/SampleHeader.yaml` |
| SignalValue scalar sample | `contracts/SignalValue.yaml`, `tools/nxdc/tests/golden.rs` |
| Sample metadata header    | `contracts/SampleHeader.yaml`, `tools/nxdc/tests/golden.rs` |
| Fixed arrays              | `valid/nested_record.yaml`                             |
| Records                   | `valid/nested_record.yaml`                             |
| Enums                     | `valid/can_frame.yaml`                                 |
| CAN frame / batch         | `valid/can_frame.yaml`, `valid/can_batch_template.yaml`, `contracts/RawCanBatch.yaml` |
| CAN heartbeat / errors    | `valid/can_heartbeat.yaml`, `valid/can_aggregate_errors.yaml` |
| Video frame / heartbeat / errors | `valid/video_frame.yaml`, `valid/video_heartbeat.yaml`, `valid/video_aggregate_errors.yaml` |
| Template params / concrete| `contracts/RawCanBatch.yaml`, `contracts/SignalValue.yaml`, `tests/codegen.rs` |
| Unbounded types           | `invalid/unbounded_*.yaml`                             |
| Malformed types           | `invalid/malformed_*.yaml`                               |
| Field metadata            | `valid/field_metadata.yaml`                              |
| Component descriptor      | `valid/component_basic.yaml`, `components/empty.yaml`, `components/deterministic_counter.yaml`, `tests/component_descriptors.rs` |
| Component ABI             | `sdk/rust/nxd-abi`, `sdk/cpp/include/nxd/abi.hpp`, `tests/abi_layout.cpp`, `tests/abi_plugin_equivalence.cpp`, `docs/component-abi.md` |
| Duplicate names           | `invalid/duplicate_*.yaml`                               |
| Invalid versions          | `invalid/invalid_version.yaml`                             |
| C++ generation            | `tests/codegen.rs`, `cmake --build build`                |
| Rust generation / ZeroCopySend | `tests/codegen.rs`                                  |
| C++/Rust golden bytes       | `tests/golden.rs` (common/CAN/video representatives), `tests/codegen.rs` (`signal_value_*_golden`) |
| Field descriptors         | `tests/codegen.rs`                                       |
| Canonical determinism     | `tests/canonical.rs`                                     |
| Layout/hash sensitivity   | `tests/canonical.rs` and `canonical::tests`             |

Run the full Rust test suite with:

```bash
RUSTFLAGS='-D warnings' cargo test --locked --workspace
```
