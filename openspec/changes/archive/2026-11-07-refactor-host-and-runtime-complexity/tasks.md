# Tasks

## 1. Inspect existing code and tests

- [x] 1.1 Read `host/src/descriptor.cpp`, `host/src/plugin.cpp`, `host/src/component.cpp`, and `runtime/recording/src/replay.cpp` to identify helper boundaries and observable behavior.
- [x] 1.2 Read existing tests for host descriptor, plugin, component, and recording replay to understand current coverage and choose where to add new helper-level tests.

## 2. Refactor `host/src/descriptor.cpp`

- [x] 2.1 Extract typed JSON helpers (`parse_string`, `parse_uint32`, `parse_bool`, `parse_array`) in an anonymous namespace and replace repetitive checks in `load_component_descriptor`; verify the file compiles.
- [x] 2.2 Split `load_component_descriptor` into per-section helpers (`parse_metadata`, `parse_ports`, `parse_parameters`, `parse_thread_roles`, `parse_supported_modes`, `parse_hardware_rt`); verify complexity drops below 15.
- [x] 2.3 Refactor `parse_ports` into a per-port parser with early-return validation; verify complexity drops below 15.
- [x] 2.4 Refactor `parse_parameters` into a per-parameter parser with early-return validation; verify complexity drops below 15.
- [x] 2.5 Add descriptor parsing unit test (`tests/host_descriptor_test.cpp`) covering valid input and failure paths; verify it passes.

## 3. Refactor `host/src/plugin.cpp`

- [x] 3.1 Split `load_plugin` into lifecycle-stage helpers; preserve existing error messages and return paths.
- [x] 3.2 Existing `nxd_host_plugin_load_test` covers each plugin loading failure stage; verify it still passes.

## 4. Refactor `host/src/component.cpp`

- [x] 4.1 Split `ComponentInstance::validate_port_bindings` into smaller validators per binding concern; preserve diagnostic order.
- [x] 4.2 Existing `nxd_host_component_lifecycle_test` and `nxd_host_port_io_test` cover port binding validation; verify they still pass.

## 5. Refactor `runtime/recording/src/replay.cpp`

- [x] 5.1 Split `ReplaySource::control` into per-command handlers; verify complexity drops below 15.
- [x] 5.2 Split `ReplaySource::step` into timeline, publish, and EOF handling helpers; verify complexity drops below 15.
- [x] 5.3 Existing `nxd_recording_recorder_replay_test` covers replay control and step behavior; verify it still passes.

## 6. Verify the change

- [x] 6.1 Run `cmake --build build -j$(nproc)` and confirm no compiler warnings or errors.
- [x] 6.2 Run `ctest --test-dir build --output-on-failure -j$(nproc)` and confirm all affected tests pass.
- [x] 6.3 Run `cmake --build build --target nxd_clang_format -j$(nproc)` and `cmake --build build --target nxd_clang_tidy -j$(nproc)` and fix any reported issues.
- [x] 6.4 Re-run the project analyzer and confirm the seven target functions no longer appear in the high-complexity list.

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
