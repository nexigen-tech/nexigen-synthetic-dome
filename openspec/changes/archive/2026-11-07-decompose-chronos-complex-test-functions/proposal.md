# Proposal

## Why

The C++ project analyzer reports several chronos test files with monolithic `main()` functions whose cyclomatic complexity exceeds the healthy threshold (>15). The largest is `runtime/chronos/tests/frame_distribution_test.cpp::main` (CCN 40), followed by `runtime/chronos/tests/mode_policy_test.cpp::main` (CCN 39) and `runtime/chronos/tests/c_api_test.cpp::main` (CCN 25). In addition, the recent refactor of `runtime/chronos/tests/state_machine_test.cpp` left one helper, `test_rejected_transactions_preserve_state()`, with CCN 29. These functions are hard to read, review, and maintain because they mix many independent test scenarios in a single block.

## What Changes

- Finish decomposing `runtime/chronos/tests/state_machine_test.cpp`:
  - Split `test_rejected_transactions_preserve_state()` into state-focused helpers such as `test_rejected_from_uninitialized()`, `test_rejected_from_initialized()`, `test_rejected_from_armed()`, `test_rejected_from_stopped()`, and `test_rejected_already_disposed()`.
  - Keep `main()` as a simple dispatcher.
- Apply the same "named helper per scenario + dispatcher `main()`" pattern to the other high-complexity chronos test files:
  - `runtime/chronos/tests/mode_policy_test.cpp`
  - `runtime/chronos/tests/frame_distribution_test.cpp`
  - `runtime/chronos/tests/c_api_test.cpp`
- Preserve every existing assertion and all observed behavior.
- Ensure all modified tests still build with the project's strict flags and pass under `ctest`.
- Apply `clang-format` to every modified file.

## Capabilities

### New Capabilities

None. This is a pure test-code refactoring.

### Modified Capabilities

None. No system behavior or spec-level requirements are changing.

## Impact

- Affected files:
  - `runtime/chronos/tests/state_machine_test.cpp`
  - `runtime/chronos/tests/mode_policy_test.cpp`
  - `runtime/chronos/tests/frame_distribution_test.cpp`
  - `runtime/chronos/tests/c_api_test.cpp`
- No API or runtime behavior changes.
- Build targets remain unchanged.
- Test exit codes may be simplified to one code per logical helper.
