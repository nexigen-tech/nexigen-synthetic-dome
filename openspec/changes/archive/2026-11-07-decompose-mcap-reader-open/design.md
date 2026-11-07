# Design

## Context

See `proposal.md - Why`. `mcap::Reader::open` currently parses the whole MCAP file inside a single `switch` statement with a cyclomatic complexity of 62. The logic is correct, but the function is hard to review, extend, and unit-test.

The MCAP dialect used by NexDome is intentionally minimal: no chunks, no compression, no checksum enforcement. Records are read sequentially from a `std::ifstream`. The reader must stay usable in a `-fno-exceptions/-fno-rtti` build and must not allocate on the hot path beyond what is already required to store decoded messages, schemas, channels, and attachments.

## Goals / Non-Goals

**Goals:**
- Reduce the cyclomatic complexity of `Reader::open` to below 15 by extracting one private/helper function per MCAP record type.
- Keep the public `Reader` API unchanged.
- Preserve the existing parsing behavior and error handling exactly.
- Make every extracted helper independently unit-testable.
- Do not introduce extra allocations on the message-reading hot path.

**Non-Goals:**
- No changes to the MCAP wire format or to `Writer`.
- No new public API surface.
- No attempt to validate or skip unknown trailing bytes inside known records (behavior is preserved as-is).
- No migration of callers needed.

## Decisions

### 1. Extract helpers as methods of a testable `ReaderImpl`

`Reader::open` will become a thin orchestrator:

```cpp
bool Reader::open(std::string_view path) noexcept {
    if (impl_ != nullptr) return false;
    impl_ = new Impl();
    if (!impl_->open_file(path)) return fail();
    if (!impl_->read_and_validate_magic()) return fail();

    bool finished = false;
    while (impl_->file.good()) {
        std::uint8_t opcode = 0;
        std::uint64_t body_size = 0;
        if (!impl_->read_record_header(opcode, body_size)) {
            if (impl_->file.eof()) break;
            return fail();
        }
        if (!dispatch_record(opcode, body_size, finished)) return fail();
        if (finished) break;
        if (impl_->is_at_zero_length_record(record_start, body_size)) break;
    }

    impl_->sort_messages();
    impl_->opened_ok = true;
    return true;
}
```

The actual record parsing moves into `ReaderImpl`:

- `read_header_body(uint64_t body_size)`
- `read_schema_body(uint64_t body_size)`
- `read_channel_body(uint64_t body_size)`
- `read_message_body(uint64_t body_size, uint64_t& order)`
- `read_attachment_body(uint64_t body_size)`
- `read_statistics_body(uint64_t body_size)`
- `skip_record_body(uint64_t body_size)`
- `read_footer_body(uint64_t body_size, bool& finished)`

**Rationale:** Each helper now has one reason to change and one branch set. The complexity of `open` drops to the dispatch table plus loop control.

**Alternatives considered:**
- Free functions in the anonymous namespace. Rejected because they would need to pass `ReaderImpl&` to every call, which is noisier and still hard to unit-test without exposing the impl type.
- Public methods on `Reader`. Rejected to avoid leaking internal parsing API.

### 2. Move `ReaderImpl` into an internal header

`runtime/recording/src/mcap_reader_impl.hpp` will declare `ReaderImpl` with all helpers. `mcap.cpp` and the new test TU both include it. `Reader` keeps its opaque `Impl*` member; the public header does not change.

**Rationale:** This makes the helpers testable without adding `friend` declarations or exposing them in the public API.

**Alternatives considered:**
- `friend` test functions. Rejected because the project’s tests are standalone executables, and maintaining friend declarations across files is fragile.
- Exposing helpers through a test-only subclass. Rejected as unnecessarily complex.

### 3. Preserve the existing allocation pattern

Helpers will continue to use the existing `read_string`/`read_bytes` helpers and will push decoded objects into the vectors already owned by `ReaderImpl`. No new temporary containers are introduced on the hot message-reading path.

**Rationale:** The user-visible behavior, including the current allocation profile, must remain identical.

### 4. Add a dedicated test executable for the helpers

A new test `runtime/recording/tests/mcap_reader_helpers_test.cpp` will exercise each helper in isolation by writing a minimal binary record body to a temporary file, positioning the stream, invoking the helper, and asserting the decoded state.

Tests will cover:
- Happy path for each record type.
- Malformed / truncated input returning `false`.
- Footer record setting `finished`.
- Unknown/summary-offset records being skipped.

A new CMake test target `nxd_recording_mcap_reader_helpers_test` will be added to `CMakeLists.txt`.

## Risks / Trade-offs

| Risk | Mitigation |
|---|---|
| Refactor accidentally changes parsing behavior. | Existing `nxd_recording_mcap`, `nxd_recording_recorder_replay`, and `nxd_validation_record_replay_validate` tests must pass unchanged. New helper tests lock down each record type. |
| Internal header leaks into unexpected TUs. | Header lives under `src/` and is only included by `mcap.cpp` and the new test. It is never installed or referenced from public headers. |
| Helper tests become coupled to binary layout. | Tests generate records through the existing `Writer` helpers where possible, so they follow the same serialization code as production. |
| Slight increase in build graph complexity. | One new test executable; acceptable for the maintainability gain. |

## Migration Plan

No migration needed. The change is source-compatible:
- Public `mcap::Reader` API unchanged.
- Existing callers continue to use `Reader::open(path)` exactly as before.
- Rollback: revert the commit restoring the monolithic `Reader::open`.

## Open Questions

None. Scope and behavior-preservation constraints are clear.
