# Tasks

## 1. Contract specs and generation

- [x] 1.1 Validate `SignalValue.yaml` and `SampleHeader.yaml` with `nxdc validate-contract` and verify generated C++ and Rust bindings compile.
- [x] 1.2 Add static assertions proving the generated `SampleHeader` matches the runtime transport user-header layout and verify `nxd_transport_headless`, `nxd_transport_iceoryx_smoke`, and `nxd_transport_iceoryx_fork` still pass.
- [x] 1.3 Update `docs/contracts.md` to list `SignalValue` and `SampleHeader` as first-class skeleton contracts if they are not already documented.

## 2. Component host and descriptor hardening

- [x] 2.1 Audit descriptor parsing to confirm every port declares contract name, major version, and `layout_hash`; add a validation diagnostic if any are missing and verify `nxd_composer_descriptor_validation` covers it.
- [x] 2.2 Verify the host initializes exactly one `nexigen::app::Infrastructure` before plugin execution and stops components before Infrastructure shutdown; confirm with `nxd_host_infrastructure` and `nxd_host_component_lifecycle`.
- [x] 2.3 Verify required thread-role failures abort arming and best-effort roles are recorded; confirm with `nxd_host_thread_roles`.

## 3. Cross-language verification

- [x] 3.1 Run `cargo test -p nxdc` golden tests (`sample_header_cross_language_golden`, `signal_value_cross_language_golden`, etc.) and verify C++ ↔ Rust byte identity.
- [x] 3.2 Run `nxd_host_port_io_golden` and `nxd_abi_plugin_equivalence` to verify C++ and Rust plugins produce matching port bytes and lifecycle behavior.
- [x] 3.3 Verify allocation-after-arm detection for C++ (`nxd_host_harness` with `nxd_test_allocating_cpp`) and Rust (`nxd_host_panic_failure` with `nxd_test_alloc_counter_rust`).

## 4. Build and test matrix

- [x] 4.1 Run `ctest` in Debug and confirm all tests pass.
- [x] 4.2 Run `ctest` in Release and confirm all tests pass.
- [x] 4.3 Run `ctest` in ASan (and UBSan if configured) and confirm no sanitizer findings.
- [x] 4.4 Run `cargo test --locked --workspace` and `cargo clippy --workspace -- -D warnings` and confirm no new warnings or failures.

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
- Verify implementation against the spec delta requirements before archiving.
