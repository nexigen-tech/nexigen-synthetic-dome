# Tasks

## 1. Refactor state_machine_test.cpp

- [x] 1.1 Extract the step-mode happy-path lifecycle block into `test_step_lifecycle_happy_path()` and verify the helper compiles.
- [x] 1.2 Extract the continuous-mode lifecycle block into `test_continuous_mode_lifecycle()` and verify the helper compiles.
- [x] 1.3 Extract the rejected-transitions block into `test_rejected_transactions_preserve_state()` and verify the helper compiles.
- [x] 1.4 Extract invalid-configuration blocks into `test_invalid_config_non_divisible_scaled_sil()`, `test_invalid_config_zero_base_interval()`, and `test_invalid_config_zero_scale_factor()` and verify the helpers compile.
- [x] 1.5 Simplify `main()` to dispatch the helpers and return a distinct exit code per failure.

## 2. Validate the refactor

- [x] 2.1 Run `cmake --build build --target nxd_chronos_state_machine_test -j$(nproc)` and verify the test executable builds without warnings.
- [x] 2.2 Run `ctest --test-dir build --output-on-failure -R nxd_chronos_state_machine` and verify the test passes.
- [x] 2.3 Run `cmake --build build --target nxd_clang_format_apply -j$(nproc)` and verify formatting is applied.

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
- Verify the archived result.
