# Tasks

## 1. Extract reader helpers into a testable internal header

- [x] 1.1 Create `runtime/recording/src/mcap_reader_impl.hpp` with the existing `ReaderImpl` struct and low-level read helpers (`read_u8`, `read_u16`, `read_u32`, `read_u64`, `read_string`, `read_bytes`, `current_offset`). Verify the file compiles when included from `mcap.cpp`.
- [x] 1.2 Move MCAP record-parsing logic out of `Reader::open` into `ReaderImpl` helper methods: `read_header_body`, `read_schema_body`, `read_channel_body`, `read_message_body`, `read_attachment_body`, `read_statistics_body`, `skip_record_body`, `read_footer_body`. Verify each helper has a cyclomatic complexity below 15 by running `lizard runtime/recording/src/mcap.cpp`.
- [x] 1.3 Rewrite `Reader::open` as a thin orchestrator that opens the file, validates magic, loops over records, dispatches to helpers, and sorts messages. Verify `nxd_recording_mcap` and `nxd_recording_recorder_replay` tests still pass.

## 2. Add unit tests for every helper

- [x] 2.1 Create `runtime/recording/tests/mcap_reader_helpers_test.cpp` covering `read_header_body`, `read_schema_body`, `read_channel_body`, `read_message_body`, `read_attachment_body`, `read_statistics_body`, `skip_record_body`, and `read_footer_body`. Verify the new test executable `nxd_recording_mcap_reader_helpers_test` builds and passes.
- [x] 2.2 Add negative tests for truncated/malformed input for each helper. Verify the test executable still passes.

## 3. Build integration

- [x] 3.1 Register `nxd_recording_mcap_reader_helpers_test` in `runtime/recording/CMakeLists.txt` with the same compile options (`-fno-exceptions`, `-fno-rtti`, `-Wall`, `-Wextra`, `-Werror`) and add it to CTest. Verify `cmake -S . -B build` configures cleanly.
- [x] 3.2 Run `cmake --build build -j$(nproc)` and confirm no new warnings or errors.

## 4. Full verification

- [x] 4.1 Run `xvfb-run ctest --test-dir build --output-on-failure -j$(nproc)` and verify all 46 tests pass.
- [x] 4.2 Run `cmake --build build --target nxd_clang_format` and `cmake --build build --target nxd_clang_tidy` and verify both lint targets pass.

## Workflow follow-up

- Archive the change after review.
