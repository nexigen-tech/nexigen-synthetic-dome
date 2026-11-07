# Tasks

## 1. Extract writer helpers into a testable internal header

- [x] 1.1 Create `runtime/recording/src/mcap_writer_impl.hpp` with `AttachmentEntry`, the existing `WriterImpl` struct, low-level write helpers, and new section helpers: `write_data_end`, `write_attachment_index_group`, `write_statistics`, `write_summary_offsets`, `write_footer_and_magic`. Verify the file compiles when included from `mcap.cpp`.
- [x] 1.2 Move finalization logic out of `Writer::close` into the `WriterImpl` helpers. Verify `Writer::close` cyclomatic complexity is below 15 by running `lizard runtime/recording/src/mcap.cpp`.
- [x] 1.3 Rewrite `Writer::close` as a sequential orchestrator. Verify `nxd_recording_mcap` and `nxd_recording_recorder_replay` tests still pass.

## 2. Add unit tests for every helper

- [x] 2.1 Create `runtime/recording/tests/mcap_writer_helpers_test.cpp` covering `write_data_end`, `write_attachment_index_group` (with and without attachments), `write_statistics`, `write_summary_offsets`, and `write_footer_and_magic`. Verify the new test executable `nxd_recording_mcap_writer_helpers_test` builds and passes.
- [x] 2.2 Add negative tests for failure paths (closed/bad stream) for each helper. Verify the test executable still passes.

## 3. Build integration

- [x] 3.1 Register `nxd_recording_mcap_writer_helpers_test` in `runtime/recording/CMakeLists.txt` with compile options `-fno-exceptions`, `-fno-rtti`, `-Wall`, `-Wextra`, `-Werror` and add it to CTest. Verify `cmake -S . -B build` configures cleanly.
- [x] 3.2 Run `cmake --build build -j$(nproc)` and confirm no new warnings or errors.

## 4. Full verification

- [x] 4.1 Run `xvfb-run ctest --test-dir build --output-on-failure -j$(nproc)` and verify all tests pass (47 after adding the new test).
- [x] 4.2 Run `cmake --build build --target nxd_clang_format` and `cmake --build build --target nxd_clang_tidy` and verify both lint targets pass.

## Workflow follow-up

- Archive the change after review.
