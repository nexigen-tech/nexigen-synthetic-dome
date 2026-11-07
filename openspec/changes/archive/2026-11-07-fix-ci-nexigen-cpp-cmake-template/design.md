# Design: Patch missing nexigen-cpp cmake config template in CI

## Root cause

`nexigen-cpp/CMakeLists.txt` invokes `configure_package_config_file(... nexigenConfig.cmake.in ...)`. The required template file exists on the local development machine at `../nexigen-ecosystem/nexigen-cpp/cmake/nexigenConfig.cmake.in`, but it is not present in the upstream GitHub repository because the remote `.gitignore` ignores `*.cmake.in` files.

## Workaround

After cloning `nexigen-cpp`, the CI workflow writes the template content directly into the expected path. The content matches the local template:

```cmake
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
find_dependency(Threads)
find_dependency(nlohmann_json)
include("${CMAKE_CURRENT_LIST_DIR}/nexigenTargets.cmake")
set_property(TARGET nexigen::core APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES nlohmann_json::nlohmann_json)
check_required_components(nexigen)
```

A `TODO` comment in the workflow step notes that this should be removed once the upstream repository commits the file or adjusts its `.gitignore`.

## Validation

The same configure command that failed in a clean environment should now succeed. After this change, CI should reach the build and test steps.
