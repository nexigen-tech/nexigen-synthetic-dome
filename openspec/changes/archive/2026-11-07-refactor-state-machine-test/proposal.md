# Proposal

## Why

The `state_machine_test.cpp` file in `runtime/chronos/tests/` contained a single `main()` function with a cyclomatic complexity of 57. This made the test difficult to read, review, and maintain. Each logical test scenario was nested inside the same block with hand-assigned return codes, increasing the risk of missing a failure path during future edits.

## What Changes

- Split the monolithic `main()` in `runtime/chronos/tests/state_machine_test.cpp` into focused, named test helper functions:
  - `test_step_lifecycle_happy_path()`
  - `test_continuous_mode_lifecycle()`
  - `test_rejected_transactions_preserve_state()`
  - `test_invalid_config_non_divisible_scaled_sil()`
  - `test_invalid_config_zero_base_interval()`
  - `test_invalid_config_zero_scale_factor()`
- Reduce `main()` to a simple dispatcher that calls each helper and returns a distinct exit code on failure.
- Preserve all existing assertions and behavior; no test logic is changed.
- Apply `clang-format` and ensure the test still builds and passes under `ctest`.

## Capabilities

### New Capabilities

None. This is a pure test-code refactoring.

### Modified Capabilities

None. No system behavior or spec-level requirements are changing.

## Impact

- Affected file: `runtime/chronos/tests/state_machine_test.cpp`
- No API or runtime behavior changes.
- Build target `nxd_chronos_state_machine_test` remains unchanged.
- Test exit codes are simplified (one code per logical test instead of one code per assertion).
