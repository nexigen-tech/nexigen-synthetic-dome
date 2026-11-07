# Proposal

## Why

The C++ project analyzer still reports several composer and recording test files with functions whose cyclomatic complexity exceeds the healthy threshold (>15):

- `runtime/composer/tests/descriptor_validation_test.cpp::main` (CCN 33)
- `runtime/composer/tests/manifest_test.cpp::main` (CCN 19)
- `runtime/composer/tests/plan_emission_test.cpp::main` (CCN 19)
- `runtime/recording/tests/mcap_writer_helpers_test.cpp::test_write_statistics` (CCN 26)
- `runtime/recording/tests/mcap_writer_helpers_test.cpp::test_write_attachment_index_group` (CCN 22)
- `runtime/recording/tests/mcap_writer_helpers_test.cpp::test_write_summary_offsets_with_attachment` (CCN 20)
- `runtime/recording/tests/recorder_replay_test.cpp::record_and_replay_test` (CCN 25)

These functions mix many independent scenarios or perform too many distinct validation steps in one block, making them hard to read and maintain.

## What Changes

- Refactor composer tests using the established "named helper per scenario + dispatcher `main()`" pattern:
  - `runtime/composer/tests/descriptor_validation_test.cpp`
  - `runtime/composer/tests/manifest_test.cpp`
  - `runtime/composer/tests/plan_emission_test.cpp`
- Refactor recording tests by splitting internally complex helpers into smaller, focused helpers while preserving the existing table-driven dispatcher where appropriate:
  - `runtime/recording/tests/mcap_writer_helpers_test.cpp`
  - `runtime/recording/tests/recorder_replay_test.cpp`
- Preserve every existing assertion and all observed behavior.
- Ensure all modified tests build with the project's strict flags and pass under `ctest`.
- Apply `clang-format` to every modified file.

## Capabilities

### New Capabilities

None. This is a pure test-code refactoring.

### Modified Capabilities

None. No system behavior or spec-level requirements are changing.

## Impact

- Affected files:
  - `runtime/composer/tests/descriptor_validation_test.cpp`
  - `runtime/composer/tests/manifest_test.cpp`
  - `runtime/composer/tests/plan_emission_test.cpp`
  - `runtime/recording/tests/mcap_writer_helpers_test.cpp`
  - `runtime/recording/tests/recorder_replay_test.cpp`
- No API or runtime behavior changes.
- Build targets remain unchanged.
