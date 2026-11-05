# Proposal

## Why

The first change established contracts, component descriptors, and a language-neutral host lifecycle, but it deliberately left out simulation time, partitions, scheduling, and system composition. Without those, components can be tested in isolation but cannot be assembled into a repeatable simulation system.

This change adds the Chronos runtime for simulation-time distribution and state control, defines Logical Execution Time (LET) boundaries for deterministic partition interaction, and delivers a minimal Composer plus a `nexigen-run` walking skeleton. The result is the smallest end-to-end path that can take a declarative system manifest, validate it, deploy components as processes, distribute time, and step a deterministic frame.

## What Changes

- Add the `runtime/chronos` module that owns simulation time, hardware FPGA time, wall time, and the execution-mode policy (RT/HIL, Scaled SIL, Free SIL, Step). Chronos distributes time to partitions and components through a stable C API and enforces deterministic time semantics for recording and replay.
- Define `ThreadSpec`-compatible partition roles and the LET rule: Direct communication is allowed only inside a partition; cross-partition traffic crosses a LET boundary with a published read time and write deadline.
- Add a minimal state-control API (initialize, arm, play, pause, step, stop, dispose) that the Composer and operator panels can drive uniformly across RT and virtual-time modes.
- Extend the component host lifecycle with runtime hooks so components can register with Chronos, receive frame contexts, and observe execution-mode transitions without depending on physical transport details.
- Add the `runtime/composer` module that reads a declarative system manifest, validates it against component descriptors, contracts, thread roles, and Chronos policies, and produces a Dome Plan describing processes, partitions, LET boundaries, and channel wiring.
- Add the `nexigen-run` walking-skeleton CLI that consumes a Dome Plan, launches a single local multi-process configuration, connects it to an iceoryx2 Blackboard, starts Chronos, and runs until completion or operator stop.
- Extend component testing with runtime-aware fixtures that can simulate Chronos time progression and LET boundary behavior without launching processes or requiring real-time threads.
- Update build and verification to include the new C++ and Rust runtime targets, Chronos unit tests, Composer validation tests, and the walking-skeleton integration test.

No existing public NXD API is changed. Chronos time types and state-control interfaces are versioned explicitly. Components built against the previous host ABI remain compatible; new runtime features require opt-in descriptor flags.

## Capabilities

### New Capabilities

- `runtime/chronos`: SimTime, WallTime, and HwFpgaTime abstractions; integer nanosecond time bases; execution-mode policy and transitions; time distribution to partitions; deterministic Step mode; state-control API; lifecycle integration with the component host.
- `runtime/let`: Partition definitions, Direct-intra-partition communication, LET-bound cross-partition communication, read-time and write-deadline semantics, and deterministic replay prerequisites.
- `runtime/composer`: System manifest schema, descriptor/contract validation, thread-role and scheduling validation, Dome Plan generation, and deployment intent for local multi-process execution.
- `tools/nexigen-run`: Walking-skeleton CLI that validates a Dome Plan, launches processes, starts Chronos, runs a single-frame demonstration, and shuts down cleanly.
- `components/host` (delta): Runtime registration, frame context with simulation time, mode-transition callbacks, and Chronos lifecycle hooks.
- `components/testing` (delta): Deterministic time-source fixture, LET-boundary test harness, and mode-transition test scenarios.

### Modified Capabilities

- `components/host`: The host frame context now includes simulation frame number, SimTime, WallTime, and HwFpgaTime where applicable, and supports an optional runtime registration step before arming.
- `components/testing`: The headless harness gains a fake Chronos source so component behavior can be tested against controlled time progression.

## Impact

- **New systems:** Chronos runtime, LET runtime semantics, Composer manifest/planner, `nexigen-run` walking skeleton, runtime-aware host hooks, and time-aware test fixtures.
- **Build:** new C++ libraries/crates for `runtime/chronos` and `runtime/composer`, new executable target for `nexigen-run`, and updated verification targets.
- **Dependencies:** local `nexigen-cpp` for `Result`/`Status`, Infrastructure, `core::ThreadSpec`, and `nexigen::chrono` host clock; iceoryx2 for local multi-process Blackboard in the walking skeleton. Live hardware FPGA time capture remains outside this change.
- **Compatibility:** generated contracts and descriptors from the previous change are consumed unchanged. Components that do not declare runtime support are driven in the default partition and mode. State-control API versions are checked before arming.
- **Hot-path performance:** steady-state frame stepping adds only time-value reads and a LET deadline check; no heap allocation, JSON parsing, or thread creation occurs on the hot path. Real-time pacer latency and iceoryx2 transport latency are measured separately and not conflated with scheduler overhead.
- **Memory:** all partition, role, and time-distribution state is allocated during arming. Step mode and virtual-time modes must not retain unbounded time history.
- **Follow-on changes:** virtual CAN/CANopen gateway, virtual video source and Qt receiver, scenario control and Tauri panel, recorder and replay, and hardware FPGA integration build on Chronos, LET, and Composer as separate changes.
