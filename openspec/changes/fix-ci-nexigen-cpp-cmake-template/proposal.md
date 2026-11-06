# Proposal: Patch missing nexigen-cpp cmake config template in CI

## Problem

After adding a clone step for the `nexigen-cpp` sibling dependency, CI builds still fail during CMake configuration. The local `nexigen-cpp` repository contains `cmake/nexigenConfig.cmake.in`, but the file is ignored by the remote `.gitignore` pattern `*.cmake.in` and therefore does not exist in the cloned repository. `nexigen-cpp/CMakeLists.txt` calls `configure_package_config_file` on this missing template, causing configure errors in CI.

## Goal

Make the CI workflow self-contained by creating the missing `nexigenConfig.cmake.in` template after cloning `nexigen-cpp`, until the issue is fixed upstream.

## Scope

In scope:
- Add a CI step that writes `cmake/nexigenConfig.cmake.in` into the cloned `nexigen-cpp` directory before CMake configure.
- Document the workaround and the upstream fix needed.

Out of scope:
- Modifying the upstream `nexigen-cpp` repository (requires separate push access).

## Success criteria

- CI configuration step succeeds for all build variants.
- Builds and tests proceed past configuration.
