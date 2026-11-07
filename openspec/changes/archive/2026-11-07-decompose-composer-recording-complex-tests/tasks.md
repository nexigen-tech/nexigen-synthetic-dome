# Tasks

## 1. Refactor descriptor_validation_test.cpp

- [x] 1.1 Extract valid-manifest check into `test_valid_manifest_passes()` and verify it compiles.
- [x] 1.2 Extract missing required port binding check into `test_missing_required_port_binding()` and verify it compiles.
- [x] 1.3 Extract missing required parameter check into `test_missing_required_parameter()` and verify it compiles.
- [x] 1.4 Extract parameter out-of-range check into `test_parameter_out_of_range()` and verify it compiles.
- [x] 1.5 Extract missing required thread role check into `test_missing_required_thread_role()` and verify it compiles.
- [x] 1.6 Extract unsupported chronos mode check into `test_unsupported_chronos_mode()` and verify it compiles.
- [x] 1.7 Extract unknown wire target check into `test_wire_references_unknown_instance()` and verify it compiles.
- [x] 1.8 Extract incompatible contracts check into `test_incompatible_contracts_between_ports()` and verify it compiles.
- [x] 1.9 Extract incompatible layout hash check into `test_incompatible_layout_hash_between_ports()` and verify it compiles.
- [x] 1.10 Extract missing port layout hash check into `test_missing_port_layout_hash()` and verify it compiles.
- [x] 1.11 Extract hardware RT requirement mismatch check into `test_hardware_rt_requirement_mismatch()` and verify it compiles.
- [x] 1.12 Extract undeclared thread role assignment check into `test_undeclared_thread_role_assignment()` and verify it compiles.
- [x] 1.13 Extract duplicate thread role assignment check into `test_duplicate_thread_role_assignment()` and verify it compiles.
- [x] 1.14 Extract multi-instance role conflict check into `test_thread_role_mapped_to_multiple_threads()` and verify it compiles.
- [x] 1.15 Update `main()` dispatcher and verify the test passes.

## 2. Refactor manifest_test.cpp

- [x] 2.1 Extract successful parse assertions into `test_basic_manifest_parses()` and verify it compiles.
- [x] 2.2 Extract invalid JSON rejection into `test_invalid_json_rejected()` and verify it compiles.
- [x] 2.3 Extract missing required field rejection into `test_missing_required_field_rejected()` and verify the test passes.

## 3. Refactor plan_emission_test.cpp

- [x] 3.1 Extract plan field assertions into `test_emitted_plan_has_expected_fields()` and verify it compiles.
- [x] 3.2 Extract JSON round-trip check into `test_plan_round_trips_through_json()` and verify the test passes.

## 4. Refactor mcap_writer_helpers_test.cpp

- [x] 4.1 Split `test_write_statistics()` into read/verify helpers and verify it compiles.
- [x] 4.2 Split `test_write_attachment_index_group()` into read/verify helpers and verify it compiles.
- [x] 4.3 Split `test_write_summary_offsets_with_attachment()` into read/verify helpers and verify the test passes.

## 5. Refactor recorder_replay_test.cpp

- [x] 5.1 Extract `record_and_replay_test()` into focused helpers for record, replay, pause/resume, and stop phases and verify they compile.
- [x] 5.2 Keep `blackboard_channel_test()` as-is (already below threshold) and verify it compiles.
- [x] 5.3 Keep `seek_and_speed_test()` as-is (already below threshold) and verify the test passes.
- [x] 5.4 Update `main()` dispatcher if needed and verify the test passes.

## 6. Validate everything

- [x] 6.1 Build all modified composer and recording test targets and verify no warnings.
- [x] 6.2 Run `ctest --test-dir build --output-on-failure -E '(video|viewer)'` for composer/recording tests and verify they pass.
- [x] 6.3 Run `cmake --build build --target nxd_clang_format_apply` and verify formatting.
- [x] 6.4 Re-run project analyzer on affected files and confirm no targeted test function exceeds CCN 15.

## Workflow follow-up

- [x] Verify the change before archiving.
- [x] Archive the change after review.
