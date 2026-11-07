# Proposal

## Why

`nxd::runtime::recording::mcap::Reader::open` currently has a cyclomatic complexity of 62, making it the most expensive function in the production code. This hinders code review, testing, and safe future extensions of the MCAP reader. Reducing its complexity will improve maintainability and make the record/replay pipeline easier to reason about.

## What Changes

- Decompose `Reader::open` into focused private helper methods, each responsible for exactly one MCAP record/section type:
  - reading the file header,
  - reading schemas,
  - reading channels,
  - reading messages,
  - reading attachments,
  - reading attachment indices / summary offsets.
- Keep the public interface of `Reader::open` unchanged.
- Preserve the existing "no allocations on the hot path" constraint.
- Add unit tests for every new private helper method.

## Capabilities

### New Capabilities

None. This is a pure internal refactor; no new user-visible capability is introduced.

### Modified Capabilities

None. The observable behavior of the MCAP reader remains identical.

## Impact

- Affected code: `runtime/recording/src/mcap.cpp`, `runtime/recording/include/nxd/runtime/recording/mcap.hpp`, and related tests in `runtime/recording/tests/`.
- Public API: unchanged.
- Dependencies: none.
- Existing tests (`nxd_recording_mcap`, `nxd_recording_recorder_replay`, `nxd_validation_record_replay_validate`) must continue to pass.
