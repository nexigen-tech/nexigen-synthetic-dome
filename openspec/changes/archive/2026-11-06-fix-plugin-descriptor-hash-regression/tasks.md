# Tasks: Fix plugin descriptor hash regression

## 1. Collect current descriptor hashes

- [x] 1.1 Verify generated sidecar hashes in `build/generated/descriptors/*.descriptor.hash` for all affected components (`deterministic_counter`, `lifecycle`, `allocating`, `alloc_counter`, `panic_step`).

## 2. Update C++ plugin descriptor hashes

- [x] 2.1 Update `descriptor_hash` in `examples/components/cpp/counter/counter.cpp` to match `deterministic_counter.descriptor.hash`; verify `nxd_host_plugin_load` and `nxd_host_harness` compile.
- [x] 2.2 Update `descriptor_hash` in `tests/components/cpp/lifecycle/lifecycle.cpp` to match `lifecycle.descriptor.hash`; verify `nxd_host_component_lifecycle` compiles.
- [x] 2.3 Update `descriptor_hash` in `tests/components/cpp/allocating/allocating.cpp` to match `allocating.descriptor.hash`; verify `nxd_host_harness` and `nxd_host_panic_failure` compile.
- [x] 2.4 Update the base `descriptor_hash` in `tests/plugins/bad_plugin.cpp` to the current `deterministic_counter.descriptor.hash`; leave the `BAD_DESCRIPTOR_HASH` variant using the intentionally wrong value.

## 3. Update Rust plugin descriptor hashes

- [x] 3.1 Update `descriptor_hash` in `examples/components/rust/counter/src/lib.rs` to match `deterministic_counter.descriptor.hash`.
- [x] 3.2 Update `descriptor_hash` in `tests/components/rust/alloc_counter/src/lib.rs` to match `alloc_counter.descriptor.hash`.
- [x] 3.3 Update `descriptor_hash` in `tests/components/rust/panic_step/src/lib.rs` to match `panic_step.descriptor.hash`.

## 4. Verify the fix

- [x] 4.1 Rebuild the affected targets: `cmake --build build --target nxd_example_counter_cpp nxd_test_lifecycle_cpp nxd_test_allocating_cpp nxd_test_plugin_bad_descriptor_hash nxd_example_counter_rust nxd_test_alloc_counter_rust nxd_test_panic_rust`.
- [x] 4.2 Run the failing host tests: `ctest --test-dir build -R 'nxd_host_plugin_load|nxd_host_component_lifecycle|nxd_host_harness|nxd_host_panic_failure' --output-on-failure`.
- [x] 4.3 Run the full suite: `ctest --test-dir build --output-on-failure -j4` and confirm 42/42 pass.

## Workflow follow-up

- Archive the change after review.
- Consider a follow-up change to automate descriptor-hash injection into plugin sources.
