# Tasks

## 1. Contracts and generation

- [x] 1.1 Add `contracts/BlackboardValue.yaml`, `contracts/BlackboardUpdate.yaml`,
  `contracts/ScenarioState.yaml`, and `contracts/ScenarioCommand.yaml`.
- [x] 1.2 Validate the new contracts with `nxdc validate-contract` and regenerate
  C++ and Rust bindings.
- [x] 1.3 Add static assertions proving the generated C++ and Rust layouts match
  for `ScenarioState` and `ScenarioCommand`.

## 2. Blackboard runtime

- [x] 2.1 Create `runtime/blackboard/` with
  `include/nxd/runtime/blackboard/blackboard.hpp` and a headless backend.
- [x] 2.2 Implement single-writer entry creation, revision counters,
  `apply_frame_no`, `layout_hash` validation, and `BlackboardUpdate` event
  emission.
- [x] 2.3 Add C++ layout tests (`tests/blackboard_layout_test.cpp`) and register
  them with CTest.
- [x] 2.4 (Skeleton) Define the iceoryx2 backend interface; full multi-process
  implementation is deferred to a later change.

## 3. `scenario_control` Rust component

- [x] 3.1 Create `examples/components/rust/scenario_control/Cargo.toml` and
  `src/lib.rs` as an ABI-1 `cdylib` using `nxd-sdk`.
- [x] 3.2 Implement lifecycle, config loading, `ScenarioCommand` input port,
  single-writer `scenario/state` output to the Blackboard, and
  `BlackboardUpdate` event emission.
- [x] 3.3 Add the component to the root `Cargo.toml` workspace and add a CMake
  custom target to build the `.so`.
- [x] 3.4 Create `components/scenario_control/component.yaml` descriptor.

## 4. C++ consumer and host integration tests

- [x] 4.1 Create a C++ test component
  (`examples/components/cpp/scenario_consumer/`) that reads `scenario/state`
  from the Blackboard and writes a `SignalValue<uint32>` output.
- [x] 4.2 Add a host integration test
  (`tests/host_scenario_control_test.cpp`) that loads `scenario_control` and
  the C++ consumer, arms them, steps, and verifies the consumer output matches
  the command applied by the Rust component.
- [x] 4.3 Verify the test passes in Debug and Release; ASan passes for the new
  tests.

## 5. Rust allocation-after-arm checks

- [x] 5.1 Add a test crate (`tests/components/rust/scenario_control_alloc/`) that
  exercises the same Blackboard write path as `scenario_control` with a counting
  global allocator.
- [x] 5.2 Add a CTest that runs the allocating test and confirms it fails if
  any allocation occurs after `arm`.

## 6. Panel source

- [x] 6.1 Create `panels/scenario-panel/` with Tauri + Svelte structure:
  `src-tauri/Cargo.toml`, `src-tauri/src/main.rs`,
  `src-tauri/src/lib.rs`, `src-tauri/tauri.conf.json`, and `src/`
  frontend files.
- [x] 6.2 Implement the Rust backend stub: `send_scenario_command` and
  `latest_scenario_state` Tauri commands.
- [x] 6.3 Implement the Svelte UI stub: display scenario state and allowlisted
  command buttons.
- [x] 6.4 The backend command path is verified through the `scenario_control`
  component tests; full GUI build requires WebKitGTK/GTK3 system libraries.
- [x] 6.5 Document the separate build recipe (panel is not part of the default
  Cargo workspace because of native dependencies).

## 7. Build and workflow wiring

- [x] 7.1 Update `CMakeLists.txt` to build `runtime/blackboard`, the new C++ test
  component, and the new tests.
- [x] 7.2 Update `Cargo.toml` workspace members to include
  `examples/components/rust/scenario_control` and
  `tests/components/rust/scenario_control_alloc`.
- [x] 7.3 Run `cargo test --workspace` and `cargo clippy --workspace -- -D warnings`
  (using the stable toolchain because the system toolchain does not ship
  clippy).
- [x] 7.4 Run `ctest` in Debug and Release; new tests also pass under ASan.

## 8. OpenSpec workflow follow-up

- [x] 8.1 Run `openspec validate add-blackboard-rust-and-tauri-skeleton`.
- [ ] 8.2 Verify all spec-delta requirements are covered by tests or
  implementation.
- [ ] 8.3 Archive the change after review and tests pass.
