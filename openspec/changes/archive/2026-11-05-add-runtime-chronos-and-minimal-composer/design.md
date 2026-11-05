# Design

## Context

See `proposal.md` — Why. The first change established contracts, descriptors, and a host/plugin lifecycle but deliberately left simulation time, scheduling, partitions, and system composition unimplemented. The repository now contains the `contracts/`, `components/`, `sdk/`, `host/`, and `tools/nxdc/` trees. This change extends those foundations downward into the runtime layer.

The target ABI is Ubuntu 26.04 on little-endian x86-64. C++ components follow local nexigen-cpp conventions: C++23, no exceptions/RTTI, `Result`/`Status` at boundaries, and no allocation or locking after arming. Rust components expose the same C ABI and binary contracts. Live hardware FPGA timing and distributed multi-node deployment are outside this change.

## Goals / Non-Goals

**Goals:**

- A deterministic simulation-time service (`runtime/chronos`) with integer nanosecond time bases, four execution modes, state control, and frame-context distribution.
- Logical Execution Time rules that make cross-partition determinism explicit and replayable.
- A declarative Composer that validates a system manifest and emits an executable Dome Plan.
- A `nexigen-run` walking skeleton that can load a Dome Plan, launch a local multi-process graph over iceoryx2, and run a single deterministic frame end-to-end.
- Minimal host and testing extensions so existing components can opt into runtime support without breaking ABI-1 compatibility.

**Non-Goals:**

- Real-time scheduling guarantees, pacer latency targets, or IRQ-level measurements.
- Live hardware FPGA time capture, CANopen gateway, video codecs, panel UI, recorder/replay, or distributed multi-node runner.
- Full ThreadSpec scheduling/affinity application inside Chronos; role validation and thread assignment remain Composer-level checks.
- `nexigen-run` production deployment, external orchestration, or service discovery.

## Design Decisions

### D1. Chronos owns three time domains and exposes them as integer nanoseconds

Chronos distinguishes `SimTime`, `WallTime`, and `HwFpgaTime` (`_HW_FPGA_TIME_BASE`). All internal time arithmetic uses signed/unsigned integer nanoseconds. Floating-point is used only for cold-path display or configuration parsing; frame budgets, deadlines, and offsets are computed exactly.

`SimTime` is the logical axis used by component algorithms and contract metadata. `WallTime` is a host monotonic clock used for timeout checks in virtual-time modes. `HwFpgaTime` is the common hardware time base when HIL is configured; in v1 it is accepted as an input to the frame context but not captured from live hardware.

### D2. Execution modes are policy values enforced before arming

The supported modes are `RealTimeHil`, `ScaledSil`, `FreeSil`, and `Step`. Mode selection is part of the Dome Plan and validated by Composer against every component descriptor's supported modes. A component declaring real-time hardware requirements cannot run in Scaled, Free, or Step mode.

Mode transitions are driven through a state-control API (`initialize`, `arm`, `play`, `pause`, `step`, `stop`, `dispose`). Transitions happen atomically with respect to component step callbacks: no step executes concurrently with a transition callback.

### D3. Frame context is owned and distributed by Chronos, borrowed by components

Chronos publishes one `RuntimeFrameContext` per partition per base frame. The context contains frame number, `SimTime`, `WallTime`, and optional `HwFpgaTime`. The host passes it to component step callbacks as a borrowed, read-only value. Components copy any values they need beyond the step; retaining a pointer to the context is forbidden.

The hot-path distribution mechanism is a single lock-free or thread-local read per partition. No heap allocation, JSON parsing, thread creation, or blocking I/O occurs after arming.

### D4. Base frame interval defaults to 1 ms and is configurable at plan level

The default base frame interval is 1 ms. Plans may declare another supported interval; Composer verifies that every component supports the chosen interval. Fast-frame intervals down to 100 µs are representable in the data model but are not required to be fully exercised in v1.

Time progression in Scaled SIL multiplies the wall interval by a positive integer scale factor. Free SIL advances `SimTime` as fast as possible without a wall-clock target. Step mode advances exactly one base frame per explicit step request.

### D5. Direct communication is intra-partition; cross-partition traffic crosses a LET boundary

A partition is a group of components that share the same frame context and may communicate directly through iceoryx2 or shared-memory channels. Cross-partition communication is declared as a LET boundary with a published read time and a write deadline.

The Composer validates that every connection either stays inside a partition or is associated with a declared LET boundary. Direct cross-partition wiring is rejected. LET deadline violations are detectable at runtime; v1 records the violation and does not promise deterministic recovery behavior beyond orderly stop.

### D6. State-control API is versioned and usable from C++, Rust, and operator tooling

The Chronos state-control API is a versioned C table similar in style to the component ABI table. It exposes initialize, arm, play, pause, step, stop, and dispose calls plus a small query/status surface. Panels, `nexigen-run`, and tests link against the same table so semantics do not diverge.

State transitions return numeric status codes. Rejected transitions do not modify runtime state. `NxdStatus` is reused from the component SDK so both layers share the same error vocabulary.

### D7. Composer validates; it does not execute

`runtime/composer` reads a system manifest, validates it against component descriptors and contracts, and emits a Dome Plan. The Dome Plan is a declar artifact consumed by runtimes and tools; it does not contain scheduler state or component code.

The Composer checks:

- descriptor presence and SDK/contract compatibility;
- required ports, parameters, and thread roles;
- contract name/major/`layout_hash` consistency across connections;
- partition membership and LET boundary declarations;
- Chronos mode, base frame interval, and hardware-time requirements;
- thread-role uniqueness and required-role coverage.

Validation failures include file/line, component instance, port, contract, role, or mode diagnostics.

### D8. Dome Plan describes local multi-process deployment intent

For v1, the Dome Plan targets a single Ubuntu 26.04 node. It describes:

- processes and their component instances;
- partition membership per instance;
- LET boundaries and cross-partition channel wiring;
- thread-role assignments and scheduling hints;
- parameter values;
- Chronos configuration (mode, base frame interval, scale factor);
- iceoryx2 blackboard/service names and executable paths.

`nexigen-run` consumes the plan directly without reparsing the manifest. Process launch, blackboard setup, and Chronos startup use the plan as source of truth.

### D9. `nexigen-run` is a walking skeleton, not a production runner

`nexigen-run` loads a Dome Plan, validates local executability, launches the described process graph, configures the iceoryx2 blackboard, starts Chronos, and runs until completion or operator stop. The v1 success criterion is a single deterministic frame executed cleanly with no leaks or hung processes.

It intentionally does not implement watchdogs, restart policy, distributed coordination, panel integration, or scenario scripting. Those are deferred to follow-on changes.

### D10. Host and testing extensions preserve ABI-1 compatibility

Runtime hooks are opt-in. A component descriptor declares runtime support, partition membership, supported modes, and required thread roles. Components that omit these declarations are placed in the default partition and driven with the default Chronos configuration without changing their existing lifecycle.

The headless test harness receives a fake Chronos source that can publish arbitrary frame-context sequences and simulate LET boundaries between partitions. Tests can verify deterministic time progression and mode transitions without launching processes or real-time threads.

### D11. C++ and Rust share the same runtime ABI

`runtime/chronos` exposes a C API used from both C++ and Rust. Generated bindings avoid C++ exceptions and Rust `Result` across the boundary. Rust runtime crates use `panic = "abort"`, matching component SDK policy. Allocation-after-arm guards inside `nexigen-run` and tests detect violations without adding production instrumentation.

### D12. Determinism is explicit and replayable

Chronos records enough information for replay from the same configuration and initial conditions to reproduce the same `SimTime` and frame-number sequence. Sources of non-determinism such as wall-clock jitter are isolated in `WallTime` and mode policy; the logical timeline depends only on the plan and state transition sequence.

## Project Structure

The change adds the following high-level layout:

```text
runtime/chronos/             C++ and Rust Chronos runtime libraries
runtime/composer/            Composer manifest/planner library and CLI
runtime/blackboard/          thin iceoryx2 adapter reused by Composer/nexigen-run
tools/nexigen-run/           walking-skeleton runner executable
components/host/             runtime registration, frame context, mode callbacks
components/testing/          fake Chronos source and LET boundary harness
```

Existing trees (`contracts/`, `components/*`, `sdk/*`, `tools/nxdc/`, `host/`) are extended rather than reorganized. The composer CLI and `nexigen-run` use `tools/nxdc` outputs as inputs but do not require the compiler at runtime.

## Risks / Trade-offs

- **integer nanoseconds overflow long-duration runs** → use 64-bit signed values; document maximum supported logical session length; keep arithmetic checked in cold path.
- **Scaled/Free SIL can outpace producers** → runtime records underruns; components are expected to keep per-frame budgets; no automatic quality-of-service adaptation in v1.
- **LET enforcement adds per-frame overhead** → deadline checks are simple integer compares; no allocation or locking on the hot path.
- **iceoryx2 adds dependency and latency variability** → walking skeleton uses a single local node; transport latency is measured and reported separately from scheduler overhead.
- **Composer validation complexity grows with manifest features** → keep v1 manifest schema small; reject unresolved references rather than guess defaults.
- **Opt-in runtime hooks could fragment component behavior** → default partition and mode ensure old components work unchanged; new capabilities require explicit descriptor declarations.
- **Process launch in `nexigen-run` is OS-specific** → target Ubuntu 26.04 only; launch details are isolated behind a small process-spawn module.
- **Hardware FPGA time is stubbed in v1** → `HwFpgaTime` enters the frame context from configuration or from a synthetic source; live capture is a follow-on change.

## Migration Plan

This change extends the source tree created by `add-contracts-and-component-sdk`. Existing components, descriptors, and generated contracts remain compatible.

1. Add `runtime/chronos` and `runtime/composer` build targets with C++ and Rust libraries.
2. Extend the component host frame context and descriptor with runtime fields and hooks.
3. Implement Composer validation and Dome Plan emission.
4. Implement `nexigen-run` walking skeleton and the `nxd-ws01` single-frame plan.
5. Extend the headless harness with fake Chronos and LET fixtures.
6. Add Chronos unit tests, Composer validation tests, and walking-skeleton integration test.
7. Run full C++ and Rust verification suites.

Rolling back removes the new `runtime/` and `tools/nexigen-run/` targets and reverts the host/testing delta files without affecting generated contracts or component descriptors.

## Open Questions

- Exact command-line interface and output format for `nexigen-run` will be finalized during implementation; the v1 requirement is only that it can run the walking-skeleton plan.
- Whether Dome Plan is JSON, YAML, or a binary artifact will be selected during implementation; both producer and consumer live in the same repository for v1.
- The precise iceoryx2 service/topic mapping for LET boundaries will be designed during implementation, constrained by the requirement that cross-partition communication is explicit.
- The exact set of supported base frame intervals and scale-factor constraints will be bounded during implementation.
- Initial `nxd-ws01` component list will be chosen from existing walking-skeleton scenarios during implementation.
