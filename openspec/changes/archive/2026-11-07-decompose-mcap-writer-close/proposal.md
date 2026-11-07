# Proposal

## Why

`nxd::runtime::recording::mcap::Writer::close` currently has a cyclomatic complexity of 39, making it the most complex function in the production code after the recent `Reader::open` refactor. Reducing its complexity will make the MCAP writer easier to maintain, review, and extend.

## What Changes

- Decompose `Writer::close` into focused private helper methods:
  - writing `DataEnd` record,
  - writing `AttachmentIndex` group,
  - writing `Statistics` record,
  - writing `SummaryOffset` records,
  - writing `Footer` record and trailing magic.
- Keep the public interface of `Writer::close` unchanged.
- Preserve the existing MCAP wire format exactly; no new fields or behavior.
- Preserve the "no allocations on the hot path" constraint.
- Add unit tests for every new private helper method.

## Capabilities

### New Capabilities

None. This is a pure internal refactor; no new user-visible capability is introduced.

### Modified Capabilities

None. The observable behavior and output format of the MCAP writer remain identical.

## Impact

- Affected code: `runtime/recording/src/mcap.cpp`, `runtime/recording/src/mcap_writer_impl.hpp` (new internal header), and related tests in `runtime/recording/tests/`.
- Public API: unchanged.
- MCAP wire format: unchanged.
- Dependencies: none.
- Existing tests (`nxd_recording_mcap`, `nxd_recording_recorder_replay`, `nxd_validation_record_replay_validate`) must continue to pass.
