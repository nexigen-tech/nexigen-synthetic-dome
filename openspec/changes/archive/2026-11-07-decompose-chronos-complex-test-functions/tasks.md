# Tasks

## 1. Finish state_machine_test.cpp decomposition

- [x] 1.1 Split `test_rejected_transactions_preserve_state()` into `test_rejected_from_uninitialized()` and verify it compiles.
- [x] 1.2 Split `test_rejected_transactions_preserve_state()` into `test_rejected_from_initialized()` and verify it compiles.
- [x] 1.3 Split `test_rejected_transactions_preserve_state()` into `test_rejected_from_armed()` and verify it compiles.
- [x] 1.4 Split `test_rejected_transactions_preserve_state()` into `test_rejected_from_stopped()` and verify it compiles.
- [x] 1.5 Split `test_rejected_transactions_preserve_state()` into `test_rejected_already_disposed()` and verify it compiles.
- [x] 1.6 Update `main()` dispatcher in `state_machine_test.cpp` and verify the test passes.

## 2. Refactor mode_policy_test.cpp

- [x] 2.1 Extract Step-mode block into `test_step_mode_advances_time()` and verify it compiles.
- [x] 2.2 Extract RealTimeHil block into `test_realtime_hil_advances_time()` and verify it compiles.
- [x] 2.3 Extract ScaledSil block into `test_scaled_sil_advances_time()` and verify it compiles.
- [x] 2.4 Extract FreeSil block into `test_free_sil_advances_time()` and verify it compiles.
- [x] 2.5 Extract play/pause blocks into `test_play_pause_rejected_in_step_mode()` and `test_play_pause_allowed_in_free_sil()` and verify the test passes.

## 3. Refactor frame_distribution_test.cpp

- [x] 3.1 Extract partition registration check into `test_registered_partitions_have_expected_ids()` and verify it compiles.
- [x] 3.2 Extract stepping assertions into `test_runtime_steps_advance_frame_and_time()` and verify it compiles.
- [x] 3.3 Extract latest-context consistency into `test_partition_latest_context_matches_runtime()` and verify it compiles.
- [x] 3.4 Extract late-registration rejection into `test_register_partition_after_arm_rejected()` and verify it compiles.
- [x] 3.5 Extract HIL hardware-time block into `test_hil_includes_hw_fpga_time()` and verify it compiles.
- [x] 3.6 Extract reader lambda into `reader_thread_fn()` and orchestration into `test_concurrent_readers_observe_consistent_frames()` and verify the test passes.

## 4. Refactor c_api_test.cpp

- [x] 4.1 Extract API table validation into `test_api_table_is_valid()` and verify it compiles.
- [x] 4.2 Extract C API lifecycle into `test_c_api_lifecycle()` and verify the test passes.

## 5. Validate everything

- [x] 5.1 Build all modified chronos test targets and verify no warnings.
- [x] 5.2 Run `ctest --test-dir build --output-on-failure -R nxd_chronos` and verify all chronos tests pass.
- [x] 5.3 Run `cmake --build build --target nxd_clang_format_apply` and verify formatting.
- [x] 5.4 Re-run project analyzer on affected files and confirm no chronos test function exceeds CCN 15.

## Workflow follow-up

- Verify the change before archiving.
- Archive the change after review.
