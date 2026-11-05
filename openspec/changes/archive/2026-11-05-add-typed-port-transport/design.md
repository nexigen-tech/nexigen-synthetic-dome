# Design

## Context

Contracts, descriptors, and the host lifecycle exist, but ABI-1 components cannot exchange data: `step` only receives `NxdFrameContext`. `add-virtual-can-skeleton` and later changes need `RawCanBatchClassical`, `CanHeartbeat`, and other typed contracts to flow between components. This change extends the ABI, SDK, host, Composer, and transport to make that possible while preserving backward compatibility with port-less plugins.

## Goals / Non-Goals

**Goals:**

- Extend ABI-1 so `step` can receive typed input/output port buffers.
- Keep legacy port-less components working unchanged.
- Provide in-process headless transport and an iceoryx2-based local multi-process transport.
- Update Composer to emit explicit port wiring and validate bindings.
- Add SDK wrappers for typed port views in C++ and Rust.
- Add tests proving cross-language golden output through ports.

**Non-Goals:**

- Distributed multi-node transport.
- External DMA buffers or codec-specific transport paths.
- Dynamic port creation after arming.
- Full LET deadline enforcement beyond metadata capture and diagnostics.
- Changing existing contracts or generated type layouts.

## Decisions

### D1. ABI-1 gains an optional `step_io` callback

The existing `NxdAbiTable` keeps `step` for port-less components. A new optional `step_io` callback is added:

```c
typedef NxdStatus (*NxdStepIoFn)(NxdComponentState* state,
                                 const NxdFrameContext* ctx,
                                 const NxdPortIoBundle* inputs,
                                 NxdPortIoBundle* outputs);
```

`NxdPortIoBundle` contains a count and an array of `NxdPortSlot`. Each slot carries the port name hash, a pointer to the typed buffer, the element count (1 for signal/event, N for stream), and the contract `layout_hash`. The host fills input slots and output loans before calling `step_io`.

**Alternative considered:** replace `step` entirely. Rejected because it would break every existing component and test.

### D2. Port buffers are pre-allocated per instance

For each component instance the host pre-allocates:

- one input buffer per input port sized to the contract sample size;
- one output loan pool per output port sized to one sample (signals/events) or a declared capacity (streams).

All memory is allocated during `initialize` or `arm`; after arming the component only reads/writes these buffers. The transport acquires and publishes loans from the pre-allocated pool.

### D3. Transport has two backends

A small `runtime/transport` library defines a `Transport` interface:

- `HeadlessTransport`: in-process channels owned by the test harness; uses pre-allocated `std::byte` buffers and deterministic frame queues.
- `IceoryxTransport`: wraps iceoryx2 publishers/subscribers keyed by logical source name for local multi-process graphs.

Both backends implement the same operations: `create_publisher(source_name, contract, pool_size)`, `create_subscriber(source_name, contract, queue_depth)`, `loan()`, `publish(loan, frame_no, sim_time)`, `take()`.

### D4. Logical source names are transport topics

A logical source name (e.g., `CAN_A`) maps to one transport topic. All output ports bound to that name publish to it; all input ports bound to that name subscribe to it. The transport enforces source isolation: `CAN_B` samples never appear on `CAN_A` subscribers.

### D5. Layout hash travels with every sample

Each transport sample header includes the contract `layout_hash`. Subscribers verify the hash matches the declared input port before delivering the sample. Mismatches are reported as validation errors before arming where possible, or as runtime drop events during stepping.

### D6. Composer plan adds Wire entries

The Composer emits a `Wire` per port binding:

```yaml
wires:
  - source_instance: virtual_can
    source_port: out
    source_name: CAN_A
    contract: RawCanBatchClassical
    contract_version: "1.0.0"
    layout_hash: <32 bytes>
    let_boundary: false
    read_time_ns: 0
    write_deadline_ns: 0
```

When source and target instances are in different partitions, `let_boundary: true` and the read/deadline fields are filled from the LET policy.

### D7. Headless harness exposes port fixtures

The existing headless harness is extended with fixture declarations:

```cpp
harness.set_input("in", std::span<const RawCanBatchClassical>{...});
auto out = harness.capture_output<RawCanBatchClassical>("out");
```

The harness allocates matching buffers, drives `step_io`, and returns captured outputs. No transport backend is required.

### D8. Rust SDK mirrors C++ port wrappers

Rust gets `nxd_sdk::Port<T>` and `nxd_sdk::OutputLoan<T>` wrappers around the C ABI. The Rust `step_io` callback receives slices and mutable references validated at compile time against generated contract types. `panic = "abort"` remains mandatory.

## Risks / Trade-offs

- **Extending ABI-1 table is a breaking layout change for newly built plugins** → `table_size` and `abi_version` minor bump let the host reject incompatible plugins; existing port-less plugins compiled against the old `step`-only table still work through the legacy callback.
- **Pre-allocating per-port buffers increases memory usage** → buffers are sized to one sample per port for signals/events; stream ports declare capacity in the descriptor.
- **Iceoryx2 dependency adds build and runtime complexity** → headless transport keeps tests isolated; iceoryx2 is used only for multi-process walking skeletons.
- **Source-name collision across contracts** → Composer validates `layout_hash` per source name; runtime drops mismatched samples and records the event.
- **LET boundary metadata adds per-sample overhead** → metadata is a small fixed-size header; hot path only copies frame number, `SimTime`, and deadline fields.

## Migration Plan

1. Extend `nxd_abi.h` with `NxdPortSlot`, `NxdPortIoBundle`, and optional `step_io`.
2. Regenerate Rust `nxd-abi` bindings.
3. Update C++ and Rust SDK with typed port wrappers.
4. Update host to allocate port buffers and call `step_io` when ports are declared.
5. Add `runtime/transport` with Headless and Iceoryx backends.
6. Update Composer to emit `Wire` entries and validate port bindings.
7. Update `runtime/let` to mark cross-partition wires.
8. Extend headless harness with port fixtures.
9. Add C++/Rust golden tests and sanitizer coverage.

Rollback removes the new ABI fields, transport library, and Composer/LET updates; port-less plugins continue to work through the original `step` path.

## Open Questions

- Whether `step_io` should be an extra field in `NxdAbiTable` or delivered through the `extension` pointer will be decided during implementation based on ABI backward-compatibility constraints.
- The exact iceoryx2 API version and publisher/subscriber configuration options will be selected during implementation.
