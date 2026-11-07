# Proposal: Fix CMake build dependencies for Rust test plugins

## Problem

In a clean build environment (such as CI), two tests fail because their required Rust plugins are not built before the test executables run:

- `nxd_abi_plugin_equivalence`: requires `nxd_example_counter_rust`, but `nxd_abi_plugin_equivalence_test` only declares a CMake build dependency on `nxd_example_counter_cpp`.
- `nxd_host_port_io_golden`: requires `nxd_test_port_io_rust`, but `nxd_host_port_io_golden_test` has no CMake build dependency on it.

Both tests only had CTest-level `DEPENDS` properties, which order tests but do not ensure the Rust shared libraries are produced during the build.

## Goal

Add the missing `add_dependencies` declarations so clean builds produce the required Rust plugins before the dependent test executables.

## Scope

In scope:
- Update `CMakeLists.txt`:
  - Add `nxd_example_counter_rust` to `add_dependencies` for `nxd_abi_plugin_equivalence_test`.
  - Add `add_dependencies(nxd_host_port_io_golden_test nxd_test_port_io_cpp nxd_test_port_io_rust)`.

Out of scope:
- Changes to Rust plugin builds.
- Changes to test logic.

## Success criteria

- A clean clone/build/test cycle passes all tests, including `nxd_abi_plugin_equivalence` and `nxd_host_port_io_golden`.
- CI builds and tests succeed after the workflow fixes for the `nexigen-cpp` dependency.
