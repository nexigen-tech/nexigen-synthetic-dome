# Design

## Context

See `proposal.md` — Why. The repository currently has no source tree or build system. The design therefore establishes a small initial project structure rather than adapting an existing implementation.

The target ABI is Ubuntu 26.04 on little-endian x86-64. C++ components follow the local nexigen-cpp rules: C++23, no exceptions/RTTI, `Result`/`Status` at the C++ boundary, and no allocation or locking after arming. Rust components use the same C ABI and binary contracts. Live iceoryx2 transport, Chronos, and composition are deliberately deferred.

The local nexigen-cpp examples confirm that `Infrastructure` is created through a policy, initialized before application ticks, and shut down after application work stops. `core::ThreadSpec` already provides validated CPU, naming, scheduling, and required/best-effort application semantics; NXD will reuse it rather than define another thread configuration model.

## Goals / Non-Goals

**Goals:**

- One schema compiler and one canonical semantic model for C++ and Rust.
- Deterministic binary compatibility checks independent of source paths and crate/binary names.
- A minimal C ABI that can remain stable while C++ and Rust SDK conveniences evolve.
- Cold-path parsing, validation, construction, and diagnostics with a fixed hot path.
- Tests that exercise the real plugin boundary without needing a simulation runtime.

**Non-Goals:**

- No live transport, service discovery, QoS planning, scheduler, or TimePage.
- No production CANopen stack, video generator/decoder, hardware driver, or panel.
- No stable backward-compatible schema evolution within one major version; incompatible changes require a new major.
- No attempt to recover a process after a Rust panic or C++ ABI violation.
- No universal dynamic payload mechanism or external DMA buffer ownership protocol.

## Decisions

### D1. One Rust schema compiler produces every language binding

A single cold-path tool, `nxdc`, parses contract YAML and component descriptors into one validated intermediate model. It emits:

- canonical schema JSON;
- C++23 headers and layout assertions;
- Rust `#[repr(C)]` types with `ZeroCopySend` support and layout assertions;
- component descriptor JSON for `nxd-host`;
- cold-path field descriptors and compatibility metadata;
- schema fixtures used by golden tests.

`nxd-host` never parses YAML. It consumes generated JSON through non-throwing nlohmann JSON handling on the cold path. This avoids separate C++ and Rust YAML parsers and their semantic drift.

The implementation uses a pinned Rust YAML parser (`yaml-rust2`) and explicit typed validation rather than deserializing directly into an unchecked semantic model. Exact crate versions are locked in the workspace lockfile.

**Alternative:** independent C++ and Rust parsers. Rejected because defaults, validation, and canonicalization would diverge.

### D2. Canonicalization and `layout_hash` are fully deterministic

`layout_hash` is the full 32-byte SHA-256 digest of a canonical UTF-8 JSON document. Canonicalization includes a format identifier, `nxd-layout-v1`, so a future algorithm revision is explicit.

The canonical document contains:

- contract name and major version;
- ordered field names and resolved field types;
- array lengths, capacities, alignment, size, and explicit padding;
- enum underlying types and numeric values;
- units, validity rules, and other semantics that affect interpretation of transported bytes.

It excludes descriptions, display labels, source paths, comments, formatting preferences, and initialization-only defaults. Object keys are lexicographically ordered; schema field arrays retain declared order; all optional structural fields receive explicit canonical defaults. Layout metadata uses integers and strings, not floating-point values.

Any change to the hash requires a contract major-version increment. Minor/patch changes are limited to documentation and other hash-neutral metadata. This deliberately conservative rule avoids claiming implicit backward compatibility.

**Alternative:** hash C++ type names or compiler debug layout. Rejected because those identities vary by binary, crate, compiler, and namespace and do not capture wire semantics.

### D3. Binary layout is explicit and conservative

Generated values use natural alignment and explicit reserved bytes, not `#pragma pack` or Rust `packed`. The initial ABI assumes little-endian x86-64, IEEE-754 binary32/binary64, and fixed-width integer types. C++ `bool`, Rust `bool`, `long`, `size_t`, pointers, and data-carrying Rust enums are excluded from wire types.

C++ enums use a declared fixed underlying integer. Rust wire enums are integer newtypes with named constants rather than Rust enum discriminants, preventing undefined behavior when newer producers send unknown values. Unknown numeric values are preserved for diagnostics and rejected only when a consumer's semantic validation disallows them.

Producers initialize all payload and reserved bytes, including padding, so golden vectors and state hashes do not depend on uninitialized memory. Generated assertions verify size, alignment, and every field offset in both languages.

### D4. Capacities are contract parameters, not hidden runtime growth

CAN support is a family of concrete instantiations with payload widths 8, 64, and 2048 bytes and an explicit per-contract batch frame capacity. `RawCanBatch` is a schema template; generated contracts have unique concrete names and hashes. There is no universal capacity chosen silently for every deployment.

Video uses the same approach: `VideoFrame` has a schema-selected maximum payload and up to four plane descriptors for known raw formats. The descriptor's `max_frame_bytes` must equal the concrete storage bound. Small fixtures exercise golden tests; large types such as FullHD RGBA can be layout-checked without constructing them on a stack.

Dynamic iceoryx2 slices and external DMA descriptors are not required. Inline bounded storage is the baseline.

### D5. Common metadata is separate from payload semantics

`SampleHeader` is generated as a fixed C-compatible value suitable for a later iceoryx2 `user_header`. It contains `frame_no`, `sim_time_ns`, `hw_time_ns`, `source_id`, `sequence`, and flags. No physical source name is embedded in each sample.

CAN and video payloads define their own `kind`, status, protocol/format metadata, and errors. Heartbeat rules are shared:

- transport sequence advances for every sample;
- content/data indices advance only for Data;
- `hw_time_ns` is the hardware formation time, not host wake-up time;
- no timestamp is derived from wall-clock by generated contract code.

The contract does not decide heartbeat `ActivitySemantics`; that remains a hardware profile concern.

### D6. Component YAML compiles into a host descriptor

`component.yaml` declares identity, SDK ABI, ports, parameter schemas, thread roles, resource limits, determinism flags, and supported modes. `nxdc` validates cross-references and emits a canonical JSON descriptor plus a descriptor hash.

The host compares that descriptor with metadata exported by the plugin. Physical bindings, endpoint names, and concrete CPU assignments are not part of a reusable component descriptor and are deferred to Composer.

### D7. The plugin ABI is a versioned C table

Each shared library exports one C entry symbol for ABI major 1. It returns a fixed-size table containing:

- ABI major/minor, table size, target ABI fingerprint, and SDK layout hash;
- component identity and required contract hashes;
- create/destroy, initialize, arm, step, and stop function pointers;
- optional extension pointer, which must be null for ABI 1.

Only fixed-width integers, C arrays, opaque pointers, byte spans `(pointer, length)`, and status codes cross the boundary. `NxdStatus` is numeric and independent of C++ exceptions or Rust `Result`.

The host owns the frame context and all input/output buffers. A borrowed buffer is valid only for the duration specified by its lifecycle callback; Stream-style retention is not introduced in this change. Each plugin creates and destroys its own state through its paired functions, ensuring allocation and deallocation use the same language runtime.

A single ABI declaration is the source of truth. `cbindgen` generates the C/C++ declaration from the Rust ABI crate; C++ and Rust layout tests compare the generated values and function-table sizes.

### D8. Frame services are host-owned and fixed-lifetime

`NxdFrameContext` contains logical frame/time fields, typed port spans, and a host-owned RNG interface. The RNG is deterministic, versioned, seeded before arming, and accessed through a fixed callback so C++ and Rust use identical sequences.

Telemetry access is through opaque host handles and small callbacks. ABI 1 supports cold diagnostic messages and fixed counter/gauge/trace operations. General runtime string formatting is not exposed. Host services and their handles remain owned by `nxd-host` and outlive every component callback that can use them.

No wall-clock, CPU, physical source name, transport endpoint, Qt/Tauri handle, or runner API is exposed in the frame context.

### D9. Host lifecycle is a single rollback chain

`nxd-host` executes this order:

1. Parse generated descriptor and apply the main/control `ThreadSpec` before workers inherit placement.
2. Create and initialize `nexigen::app::Infrastructure` through an NXD policy.
3. Load the shared library and validate its ABI, target fingerprint, component identity, and contracts.
4. Create and initialize the component.
5. Arm it and apply the thread specification on the thread that will execute the headless driver.
6. Drive the requested frames or run until a stop is requested.
7. Stop and destroy the component, unload the plugin, then flush and shut down Infrastructure.

Failure at any stage runs only the applicable reverse cleanup stages. `step` failure records its numeric status and frame number without formatting and prevents the next step. Real partition scheduling and runner restart policy are not part of this host version.

### D10. Rust uses abort-on-panic and explicit allocation tests

Rust component libraries are built with `panic = "abort"`. No `catch_unwind` is used across the component ABI. Panic tests run in a child process and verify abnormal termination and absence of subsequent frames.

Rust components return numeric statuses and use no `unwrap`/`expect` in SDK hot-path wrappers. A test component crate installs a counting `GlobalAlloc`; after arming, any allocation is recorded. Production components do not install that allocator.

C++ test binaries use test-only global allocation counters and an armed-state guard. The production host has no per-frame allocation instrumentation. Locking is prevented by the SDK surface and code review/static checks rather than by adding a runtime mutex detector.

### D11. The headless harness uses real ABI calls, not a parallel mock lifecycle

The harness loads the same plugins and invokes the same table as `nxd-host`, but supplies frame contexts and input/output buffers directly. It provides:

- cold initialization from generated descriptors;
- deterministic seed and frame control;
- output capture and per-frame hashes;
- guarded buffer regions around inputs/outputs to detect out-of-bounds writes;
- C++ and Rust allocation-after-arm checks;
- lifecycle injection for initialize/arm/step/stop failures;
- child-process panic testing;
- empty-component ABI overhead measurement.

The benchmark reports only call overhead and build/host details. No scheduler or transport latency is inferred.

### D12. Initial project structure keeps cold and hot code separate

The implementation is organized approximately as follows:

```text
 contracts/                 YAML schemas and schema templates
 components/                component descriptors and ABI definition
 tools/nxdc/                Rust schema/descriptor compiler
 sdk/cpp/                   C++ typed views and SDK helpers
 sdk/rust/                  Rust ABI, typed views, and SDK helpers
 host/                      nxd-host and Infrastructure policy
 examples/components/       empty and deterministic C++/Rust plugins
 tests/                     C++, Rust, golden, lifecycle, and integration tests
 CMakeLists.txt             C++ build and test orchestration
 Cargo.toml / Cargo.lock    pinned Rust workspace
```

C++ parsing, JSON, and diagnostic bodies remain in compiled cold-path units. Generated hot-path types are fixed-layout values with no unnecessary standard-library machinery.

## Risks / Trade-offs

- **Rust YAML/parser and generator dependencies increase the initial toolchain surface** → Pin all versions, keep them cold-path-only, and make generated code compilable without the generator installed.
- **Exact major-plus-hash compatibility rejects potentially compatible additive changes** → Accept conservative behavior for ABI 1; design explicit schema evolution later rather than risk silent incompatibility.
- **Natural alignment can waste memory in large batches** → Make padding and total size visible in generated reports; do not use unsafe packing to save memory.
- **A panic-aborting Rust plugin terminates the whole host process** → Use process isolation for tests and later runner supervision; never pretend the host can continue safely.
- **Allocation guards cannot prove that every dependency is allocation-free** → Cover representative C++/Rust plugins, run sanitizers, and keep third-party runtime dependencies out of the component hot path.
- **C ABI does not detect every lifetime violation in native code** → Add guarded buffers, lifetime documentation, ASan, and SDK wrappers that make borrows explicit.
- **Large contract types can overflow stacks even when bounded** → Allocate test and runtime buffers from pre-initialized owner storage; use type-only layout tests for very large frames.
- **Generator and host could disagree on descriptor semantics** → Compare descriptor hashes with plugin metadata and test one canonical fixture through both paths.
- **Initial build system has no legacy compatibility constraints but can easily become inconsistent** → Use one CMake entry point, one Cargo workspace, and one documented verification matrix from the start.

## Migration Plan

This is a greenfield change, so no existing NXD consumers require migration.

1. Establish the build workspaces and canonical schema/ABI fixtures.
2. Introduce generated contracts and cross-language layout tests.
3. Introduce component descriptors, the ABI table, and host lifecycle.
4. Add headless C++ and Rust examples and allocation/panic tests.
5. Pin generated outputs and verify that clean builds reproduce identical schemas and hashes.

Rolling back the change removes the new source tree without affecting existing project code. Generated consumers must remain with a matching generator/schema major; older generators are not expected to read newer canonical schema versions.

## Open Questions

- Exact patch versions of Rust and iceoryx2 dependencies will be selected and locked during implementation without changing the architecture.
- Whether generated C++/Rust outputs are committed, produced only at build time, or both for release archives will be decided by repository distribution policy; deterministic regeneration tests are required in either case.
- A general deferred hot-path logging vocabulary for components is deferred to the runtime change. ABI 1 exposes only the fixed telemetry operations needed by the examples and tests.
