# Design: Fix CMake build dependencies for Rust test plugins

## Root cause

CTest `DEPENDS` properties only control the order in which tests are executed. They do not instruct CMake's build system to produce a target's outputs. For Rust plugins built via `add_custom_target`, an explicit `add_dependencies` call is required so that building a test executable also triggers the cargo build.

## Fix

In `CMakeLists.txt`:

1. Change:
   ```cmake
   add_dependencies(nxd_abi_plugin_equivalence_test nxd_abi_header nxd_example_counter_cpp)
   ```
   to:
   ```cmake
   add_dependencies(nxd_abi_plugin_equivalence_test nxd_abi_header nxd_example_counter_cpp nxd_example_counter_rust)
   ```

2. Add before `add_test(NAME nxd_host_port_io_golden ...)`:
   ```cmake
   add_dependencies(nxd_host_port_io_golden_test nxd_test_port_io_cpp nxd_test_port_io_rust)
   ```

## Verification

A clean build in an isolated directory (clone, configure, build, test) should now pass all 42 tests. This was reproduced locally by mimicking the CI environment.
