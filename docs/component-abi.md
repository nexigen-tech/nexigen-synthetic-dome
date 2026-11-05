# NXD Component ABI-1

This document describes the versioned C ABI used between the NXD host and
plugin shared libraries.  ABI-1 is intentionally small: it exposes only opaque
handles, numeric statuses, a frame context, and a table of function pointers.
The same ABI is usable from C, C++, and Rust.

## Target

ABI-1 targets Ubuntu 26.04 on little-endian x86-64.  C++ components use C++23
with `-fno-exceptions` and `-fno-rtti`.  Rust components are built with
`panic = "abort"` so no Rust panic unwinds across the C boundary.

## Versioning

The ABI table begins with a three-part `NxdAbiVersion` and a `table_size`.
The host rejects a plugin when either value differs from the expected ABI-1
values.  A 32-byte `fingerprint` (`NXD_ABI_1_TARGET`) identifies the target
layout; a mismatch also aborts loading.  The `sdk_layout_hash` field is
reserved for binding/SDK fingerprinting in future ABI revisions and is
expected to be zero-filled for ABI-1.

Incompatible ABI changes require a new major version.  Compatible additions
may use a new minor version but must keep `table_size` unchanged for the same
major version.

## Status codes

Every lifecycle function returns a `NxdStatus`:

| Constant                         | Value | Meaning                                    |
|----------------------------------|-------|--------------------------------------------|
| `NXD_OK`                         | 0     | Success.                                   |
| `NXD_ERROR_GENERAL`              | 1     | Unspecified error.                         |
| `NXD_ERROR_INVALID_ARGUMENT`     | 2     | Null pointer or malformed argument.        |
| `NXD_ERROR_NOT_SUPPORTED`        | 3     | Capability not supported by the plugin.    |
| `NXD_ERROR_ALREADY_INITIALIZED`  | 4     | Component was already initialized.         |
| `NXD_ERROR_NOT_INITIALIZED`      | 5     | Component was not initialized/armed.       |
| `NXD_ERROR_OUT_OF_MEMORY`        | 6     | Allocation failed (cold path only).        |
| `NXD_ERROR_ABI_MISMATCH`         | 7     | ABI/fingerprint/descriptor mismatch.       |

## Lifecycle

The host drives every plugin through the following ordered stages.  A failure
in any stage prevents later stages and triggers reverse teardown.

1. **Load** the shared library and resolve `nxd_abi_entry`.
2. **Validate** the ABI table, fingerprint, component identity, descriptor
   hash, and declared contracts/resources.
3. **Create** the component state by calling `create`.
4. **Initialize** the component with a UTF-8 configuration blob by calling
   `initialize`.
5. **Arm** the component before the first frame by calling `arm`.
6. **Step** repeatedly by calling `step` with monotonically increasing frame
   numbers.
7. **Stop** the component by calling `stop`.
8. **Destroy** the state by calling `destroy`.

`create`/`destroy` and `initialize`/`arm`/`step`/`stop` are paired.  The host
never calls lifecycle functions concurrently for the same component instance.

## The ABI table

`NxdAbiTable` is a plain `#[repr(C)]` / `extern "C"` struct.  All lifecycle
function pointers must be non-null for ABI-1.  The optional `step_io` pointer
is null for legacy port-less plugins; port-aware plugins set it to a valid
callback.

```c
typedef struct {
    NxdAbiVersion abi_version;
    size_t table_size;
    NxdAbiFingerprint fingerprint;
    NxdAbiFingerprint sdk_layout_hash;
    char component_name[64];
    NxdAbiVersion component_version;
    NxdAbiFingerprint descriptor_hash;
    NxdCreateFn create;
    NxdDestroyFn destroy;
    NxdInitializeFn initialize;
    NxdArmFn arm;
    NxdStepFn step;
    NxdStopFn stop;
    NxdStepIoFn step_io;
} NxdAbiTable;
```

For ABI-1, `table_size` must equal `sizeof(NxdAbiTable)` (256 bytes) and the
alignment must equal the platform pointer size (8 bytes on x86-64).

## Frame context

`NxdFrameContext` is owned by the host and is valid only for the duration of
the `arm` or `step` callback.  The component must not retain pointers from the
context after returning.

```c
typedef struct {
    uint64_t frame_no;
    int64_t sim_time_ns;
    int64_t hw_time_ns;
    uint32_t seed_version;
    uint64_t (*rng)(void* state);
    void* rng_state;
} NxdFrameContext;
```

- `frame_no` starts at zero for the first armed frame and increments by one
  per `step`.
- `sim_time_ns` is the deterministic simulation time.
- `hw_time_ns` is a hardware timestamp if available; it is provided for
  diagnostic correlation only and must not affect deterministic output.
- `seed_version` identifies the current deterministic seed.
- `rng` is an optional deterministic callback.  If null, RNG is not armed.

The context exposes no wall-clock time, physical channels, CPU affinity, or
endpoints.

## Typed port I/O

Port-aware plugins declare input and output ports in their component
descriptor.  During `arm` the host validates every required port against the
plan binding, allocates a typed buffer per port, and passes the buffers to the
optional `step_io` callback on every frame:

```c
typedef struct {
    uint64_t port_name_hash;
    void* data;
    uintptr_t count;
    uintptr_t element_size;
    NxdAbiFingerprint layout_hash;
} NxdPortSlot;

typedef struct {
    uintptr_t count;
    NxdPortSlot* slots;
} NxdPortIoBundle;

typedef NxdStatus (*NxdStepIoFn)(
    NxdComponentState* state,
    const NxdFrameContext* ctx,
    const NxdPortIoBundle* inputs,
    NxdPortIoBundle* outputs);
```

The host fills `port_name_hash` with the FNV-1a hash of the port name,
`data` with a writable buffer sized to `count * element_size` bytes, and
`layout_hash` with the 32-byte contract layout hash from the descriptor.
Input slots are read-only from the plugin's point of view; output slots are
writable loans that the plugin fills and returns.

The host rejects arming with `NXD_ERROR_ABI_MISMATCH` when:

* a required port has no binding,
* a binding's direction does not match the descriptor,
* the binding's `element_size` or `count` is zero,
* the binding's `layout_hash` differs from the descriptor's `contract_hash`,
* the plugin exports no `step_io` callback but the descriptor declares ports.

## Ownership and buffer lifetimes

- The host owns the `NxdAbiTable` and the shared library mapping; the table
  pointer remains valid until `dlclose`.
- The host owns the `NxdFrameContext` and any buffers reachable through it;
  they are valid only inside the callback.
- The plugin owns the opaque `NxdComponentState` returned by `create`.  The
  host passes it back to every lifecycle function except `create`.  `destroy`
  releases the state and must tolerate `null` gracefully if the host chooses to
  call it.
- Telemetry handles (`NxdTelemetryHandle`) are owned by the host and are
  reserved for future ABI revisions.

## Panic rules

- C++ plugins must not throw exceptions.  All errors are reported through
  status codes.
- Rust plugins use `panic = "abort"`.  A panic terminates the plugin process;
  the host detects the failure and does not continue stepping.

## SDK wrappers

Thin, zero-allocation wrappers are provided for both languages:

- C++: `sdk/cpp/include/nxd/sdk.hpp` provides `nxd::FrameContext`, `nxd::Rng`,
  and `nxd::Port<T>`.
- Rust: `sdk/rust/nxd-sdk` provides `FrameContext`, `Rng`, `Port<'a, T>`, and
  the `Component` trait.

The wrappers do not allocate or lock on the hot path.

## Minimal example plugins

Complete deterministic counter plugins that produce identical state
transitions are checked into the repository:

- `examples/components/cpp/counter/counter.cpp`
- `examples/components/rust/counter/src/lib.rs`

Both implement `create`, `destroy`, `initialize`, `arm`, `step`, and `stop`,
fill the ABI table, and advance an offset using the deterministic RNG.  The
`nxd_abi_plugin_equivalence` CTest loads both shared libraries and verifies
that matching frame contexts produce identical offsets.

## Regenerating the C header

The canonical ABI C header is generated from `sdk/rust/nxd-abi` with
`cbindgen`:

```bash
cbindgen --crate nxd-abi --output sdk/rust/nxd-abi/nxd_abi.h \
         --config sdk/rust/nxd-abi/cbindgen.toml
```

CMake runs this command automatically as the `nxd_abi_header` target.
