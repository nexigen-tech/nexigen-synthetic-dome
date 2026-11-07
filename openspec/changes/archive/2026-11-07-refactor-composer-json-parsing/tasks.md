# Tasks

## 1. Inspect existing tests and headers

- [x] 1.1 Read `runtime/composer/include/nxd/runtime/composer/plan.hpp`, `manifest.hpp`, and `descriptor_validation.hpp` to confirm public types and ensure helpers can remain in anonymous namespaces.
- [x] 1.2 Read existing composer tests under `runtime/composer/tests/` to understand current coverage and choose where to add new helper-level tests.

## 2. Add shared parsing utilities

- [x] 2.1 Add a generic `parse_array<T, ParseFn>` helper in `runtime/composer/src/plan.cpp` and replace the four repetitive array loops in `DomePlan::from_json`; verify the file still compiles.
- [x] 2.2 Add the same `parse_array<T, ParseFn>` helper in `runtime/composer/src/manifest.cpp` and replace the five repetitive array loops in `SystemManifest::from_json`; verify the file still compiles.

## 3. Refactor `runtime/composer/src/plan.cpp`

- [x] 3.1 Extract `DomePlan::from_json` into per-section helpers (`partitions_from_json`, `let_boundaries_from_json`, `wires_from_json`, `processes_from_json`, `thread_role_assignments_from_json`, `parameter_assignments_from_json`) using early-return `bool` semantics; verify with `cmake --build build --target nxd_composer -j$(nproc)`.
- [x] 3.2 Refactor `validation_rule_from_json` using typed optional-field extractors and confirm cyclomatic complexity is below 15 by re-running `python3 .opencode/skills/cpp-project-analizer/analyzer.py .`.

## 4. Refactor `runtime/composer/src/manifest.cpp`

- [x] 4.1 Extract `SystemManifest::from_json` into per-section helpers (`component_refs_from_json`, `instances_from_json`, `partitions_from_json`, `wires_from_json`, `let_boundaries_from_json`, `processes_from_json`) using early-return `bool` semantics; verify with the composer build target.
- [x] 4.2 Confirm `SystemManifest::from_json` cyclomatic complexity is below 15 by re-running the analyzer.

## 5. Refactor `runtime/composer/src/descriptor_validation.cpp`

- [x] 5.1 Split `validate_instance` into `validate_required_ports`, `validate_parameters`, `validate_required_thread_roles`, `validate_supported_mode`, `validate_hardware_rt_requirement`, and `validate_assigned_thread_roles`; keep each helper `noexcept` and preserve diagnostic order.
- [x] 5.2 Compose the new validators inside `validate_instance` and confirm the file builds and complexity is below 15.

## 6. Add regression tests

- [x] 6.1 Add unit tests for the new `plan.cpp` helpers covering valid input and each failure path (missing field, wrong type, malformed element).
- [x] 6.2 Add unit tests for the new `manifest.cpp` helpers covering valid input and each failure path.
- [x] 6.3 Add unit tests for the split `descriptor_validation.cpp` validators covering required ports, parameters, thread roles, supported modes, hardware-RT constraints, and assigned-role duplicates.

## 7. Verify the change

- [x] 7.1 Run `cmake --build build -j$(nproc)` and confirm no compiler warnings or errors.
- [x] 7.2 Run `ctest --test-dir build --output-on-failure -j$(nproc)` and confirm all composer and host tests pass.
- [x] 7.3 Run `cmake --build build --target nxd_clang_format -j$(nproc)` and `cmake --build build --target nxd_clang_tidy -j$(nproc)` and fix any reported issues.
- [x] 7.4 Re-run the project analyzer and confirm the four target functions no longer appear in the high-complexity list.

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
