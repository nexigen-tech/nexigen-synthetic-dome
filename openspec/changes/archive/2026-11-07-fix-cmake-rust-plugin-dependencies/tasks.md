# Tasks: Fix CMake build dependencies for Rust test plugins

## 1. Fix dependencies

- [x] 1.1 Add `nxd_example_counter_rust` to `add_dependencies` for `nxd_abi_plugin_equivalence_test`.
- [x] 1.2 Add `add_dependencies(nxd_host_port_io_golden_test nxd_test_port_io_cpp nxd_test_port_io_rust)`.

## 2. Verify locally

- [x] 2.1 Run a clean configure/build/test in an isolated directory and confirm 42/42 tests pass.
- [x] 2.2 Rebuild the local `build` directory and confirm 42/42 tests pass.

## 3. Commit and push

- [x] 3.1 Commit the CMake fix.
- [x] 3.2 Push and confirm CI re-runs.

## 4. Archive change

- [x] 4.1 Mark tasks complete.
- [x] 4.2 Run `openspec archive` for `fix-cmake-rust-plugin-dependencies`.
