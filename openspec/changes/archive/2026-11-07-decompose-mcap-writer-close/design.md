# Design

## Context

See `proposal.md - Why`. `mcap::Writer::close` currently finalizes the MCAP file in a single function with cyclomatic complexity 39. It writes the `DataEnd`, `AttachmentIndex`, `Statistics`, `SummaryOffset`, and `Footer` records, plus the trailing magic. The logic is correct but monolithic.

The writer shares the same environment constraints as the reader: `-fno-exceptions/-fno-rtti`, C++23, and minimal allocations.

## Goals / Non-Goals

**Goals:**
- Reduce the cyclomatic complexity of `Writer::close` to below 15.
- Extract one private/helper method per finalization section (`DataEnd`, `AttachmentIndex`, `Statistics`, `SummaryOffset`, `Footer`).
- Keep the public `Writer` API unchanged.
- Preserve the exact MCAP wire format and the "no allocations on the hot path" rule.
- Make every extracted helper independently unit-testable.

**Non-Goals:**
- No changes to `Reader` or to the MCAP format.
- No new public API.
- No additional allocation during finalization beyond what already happens.

## Decisions

### 1. Extract helpers as methods of a testable `WriterImpl`

`Writer::close` will become a sequential orchestrator:

```cpp
bool Writer::close() noexcept {
    if (impl_ == nullptr) return false;
    if (impl_->finalized || !impl_->file.is_open()) return impl_->finalized;

    if (!impl_->write_data_end()) return false;

    const std::uint64_t summary_start = impl_->current_offset();

    std::uint64_t attachment_index_group_start = 0;
    std::uint64_t attachment_index_group_length = 0;
    if (!impl_->write_attachment_index_group(attachment_index_group_start,
                                             attachment_index_group_length)) {
        return false;
    }

    std::uint64_t stats_group_start = 0;
    std::uint64_t stats_group_length = 0;
    if (!impl_->write_statistics(stats_group_start, stats_group_length)) {
        return false;
    }

    const std::uint64_t summary_offset_start = impl_->current_offset();
    if (!impl_->write_summary_offsets(attachment_index_group_start,
                                      attachment_index_group_length,
                                      stats_group_start,
                                      stats_group_length)) {
        return false;
    }

    if (!impl_->write_footer_and_magic(summary_start, summary_offset_start)) {
        return false;
    }

    return true;
}
```

Each helper owns exactly one MCAP finalization section.

### 2. Move `WriterImpl` into an internal header

`runtime/recording/src/mcap_writer_impl.hpp` will declare `WriterImpl`, `AttachmentEntry`, and the helper methods. `mcap.cpp` and the new test TU both include it. The public `mcap.hpp` does not change.

**Rationale:** Same as for `ReaderImpl`: testability without exposing implementation details or using `friend`.

### 3. Preserve byte-for-byte output

Helpers will compute the same sizes and call the same low-level `write_*` primitives as today. No padding, field order, or constant value changes.

### 4. Add a dedicated test executable for the helpers

`runtime/recording/tests/mcap_writer_helpers_test.cpp` will exercise each helper by writing a section to a temp file and reading it back with `ReaderImpl` low-level helpers. Tests cover:
- happy path for `DataEnd`, `AttachmentIndex`, `Statistics`, `SummaryOffset`, `Footer`;
- empty attachment list (no AttachmentIndex/SummaryOffset for it);
- failure paths where the stream is closed before writing.

A new CMake test target `nxd_recording_mcap_writer_helpers_test` will be added.

## Risks / Trade-offs

| Risk | Mitigation |
|---|---|
| Refactor changes the written byte stream. | Compare MCAP files produced by existing tests before/after; existing `nxd_recording_mcap` and round-trip tests must pass. |
| Internal header leaks. | Header lives under `src/` and is only included by `mcap.cpp` and the new test. |
| Helper tests become coupled to binary layout. | Tests reuse `ReaderImpl` read helpers from `mcap_reader_impl.hpp` where possible, so they validate against the same parsing code. |

## Migration Plan

No migration needed. The public API and output format are unchanged. Rollback: revert the commit.

## Open Questions

None.
