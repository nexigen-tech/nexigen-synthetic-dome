# Tasks

## 1. Chronos runtime core

- [x] 1.1 Create `runtime/chronos/` C++ and Rust crate/library targets, wire them into the root build, and verify both compile in Debug and Release with C++23 and the workspace's Rust edition.
- [x] 1.2 Define `SimTime`, `WallTime`, `HwFpgaTime`, and `Duration` integer-nanosecond types with exact arithmetic and C-compatible layout, and verify cross-language size, alignment, and conversion invariants.
- [x] 1.3 Implement the Chronos state machine (`initialize`, `arm`, `play`, `pause`, `step`, `stop`, `dispose`) with atomic transition ordering and reject invalid transitions, and verify every disallowed transition returns an error status without modifying state.
- [x] 1.4 Implement execution-mode policy (`RealTimeHil`, `ScaledSil`, `FreeSil`, `Step`) with mode-specific time progression and scale-factor validation, and verify each mode advances SimTime according to its rule.
- [x] 1.5 Implement frame-context distribution to partitions with frame number, SimTime, WallTime, and optional HwFpgaTime, and verify all components in a partition observe identical values for the same frame.
- [x] 1.6 Add a versioned C state-control API table for Chronos and generate matching Rust bindings, and verify C++ and Rust callers can initialize, arm, step, and stop the same runtime instance.
- [x] 1.7 Add Chronos unit tests covering mode transitions, time arithmetic, frame distribution, and state invariants, and run them under AddressSanitizer.

## 2. Logical Execution Time and partitions

- [x] 2.1 Define partition and LET-boundary data types with published read times and write deadlines, and verify they serialize correctly in the Dome Plan.
- [x] 2.2 Implement intra-partition direct wiring validation: Composer accepts direct connections only between ports in the same partition.
- [x] 2.3 Implement cross-partition LET-boundary validation: Composer requires an explicit LET boundary for any wire spanning partitions.
- [x] 2.4 Implement runtime LET deadline checks with recordable violations, and verify a late write to a cross-partition port is detected while intra-partition writes are unaffected.
- [x] 2.5 Document the LET rules in `docs/let.md` and verify every rule has a Composer validation scenario and a runtime unit test.

## 3. Composer manifest and Dome Plan

- [x] 3.1 Define the v1 system manifest schema (components, instances, partitions, wiring, parameters, thread roles, Chronos config) and validate it against representative fixtures.
- [x] 3.2 Implement Composer validation against component descriptors: required ports, parameters, thread roles, supported modes, contract identities, and layout hashes.
- [x] 3.3 Implement Composer contract-compatibility checks across connected ports by name, major version, and layout hash, and verify mismatches produce diagnostics naming both ports.
- [ ] 3.4 Implement Chronos policy validation: execution mode, base frame interval, and hardware-time requirements must be supported by every involved descriptor.
- [ ] 3.5 Implement thread-role validation: required roles are assigned, no role is assigned to more than one thread, and scheduling/affinity declarations are internally consistent.
- [ ] 3.6 Implement Dome Plan emission containing processes, partitions, LET boundaries, wiring, thread-role assignments, parameters, and Chronos configuration, and verify `nexigen-run` can consume it without reparsing the manifest.
- [ ] 3.7 Add Composer validation tests for each failure class and confirm diagnostics include file/line, component instance, port, contract, role, or mode context.

## 4. `nexigen-run` walking skeleton

- [ ] 4.1 Create `tools/nexigen-run/` executable target and wire it into the build.
- [ ] 4.2 Implement Dome Plan loading and local executability checks (executable paths exist, needed thread roles can be assigned, local iceoryx2 node is reachable).
- [ ] 4.3 Implement process group launch on Ubuntu 26.04 using the Dome Plan, and verify child processes inherit the correct environment and terminate cleanly on shutdown.
- [ ] 4.4 Integrate the iceoryx2 blackboard adapter for local multi-process pubs/subs based on the Dome Plan wiring.
- [ ] 4.5 Implement Chronos startup from plan configuration and connect it to the local blackboard so partitions receive frame contexts.
- [ ] 4.6 Implement the walking-skeleton scenario: initialize, arm, step at least one frame, and stop without leaks or hung processes.
- [ ] 4.7 Add an integration test that runs `nexigen-run` with a committed `nxd-ws01` plan and verifies deterministic frame completion and clean shutdown.

## 5. Component host runtime integration

- [ ] 5.1 Extend the component descriptor with optional partition, supported modes, required thread roles, and runtime-support flag without breaking existing descriptors.
- [ ] 5.2 Extend the host frame context with frame number, SimTime, WallTime, and optional HwFpgaTime, preserving ABI-1 table layout for components that do not opt in.
- [ ] 5.3 Implement runtime registration during component initialization and reject registration changes after arming.
- [ ] 5.4 Implement mode-transition callbacks and guarantee no step callback executes concurrently with a transition callback.
- [ ] 5.5 Enforce read-only frame context ownership and add SDK checks that prevent retaining a context pointer past the step.
- [ ] 5.6 Add host integration tests that exercise runtime registration, frame context values, and mode transitions for C++ and Rust example components.

## 6. Component testing runtime fixtures

- [ ] 6.1 Implement a fake Chronos source that publishes configurable frame-context sequences with exact SimTime increments from headless tests.
- [ ] 6.2 Add LET-boundary simulation to the headless harness with read times and write deadlines, and verify late writes are detected.
- [ ] 6.3 Add mode-transition test scenarios (initialize, arm, play, pause, step, stop) and verify transition ordering without interleaving step callbacks.
- [ ] 6.4 Extend allocation-after-arm detection to runtime callbacks and frame-context handling.
- [ ] 6.5 Implement a deterministic replay fixture that records and replays frame-context and LET-boundary event sequences for regression tests.
- [ ] 6.6 Add C++ and Rust headless tests that demonstrate time-aware behavior without iceoryx2, Chronos processes, or real-time threads.

## 7. Integration and verification

- [x] 7.1 Run the complete C++ Debug and Release suites and verify all correctness tests pass with C++23, `-fno-exceptions`, and `-fno-rtti`.
- [x] 7.2 Run ThreadSanitizer, AddressSanitizer, and UBSan suites and verify no races, leaks, bounds errors, or undefined behavior.
- [x] 7.3 Run Rust `cargo fmt --check`, `cargo clippy -- -D warnings`, debug tests, and release tests, and verify every check succeeds.
- [ ] 7.4 Run the Composer validation suite and the `nexigen-run` walking-skeleton integration test and verify clean execution.
- [x] 7.5 Verify that components built against the ABI-1 plugin table without runtime support still pass the existing headless harness tests unchanged.
- [x] 7.6 Update `ARCHITECTURE_EXPLORER.md`, `DECISIONS_EXPLORER.md`, and `WALKING_SKELETON_EXPLORER.md` only if implementation reveals material changes; otherwise confirm they remain accurate.
- [x] 7.7 Run `openspec validate add-runtime-chronos-and-minimal-composer --strict` and verify every proposal capability has a corresponding spec, every requirement has a scenario, all tasks are complete, and the design has no unresolved scope-changing decision.
