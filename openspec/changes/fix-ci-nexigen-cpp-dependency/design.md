# Design: Fix CI workflow — clone missing nexigen-cpp dependency

## Root cause

`CMakeLists.txt` hardcodes the path to the local `nexigen-cpp` repository as a sibling directory:

```cmake
set(NEXIGEN_CPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../nexigen-ecosystem/nexigen-cpp" ...)
```

Local development keeps both repositories side-by-side, but GitHub Actions only checks out `nexigen-synthetic-dome`. Without the sibling, CMake cannot add the subdirectory and the build fails.

## Fix

Add a step immediately after checkout that creates `../nexigen-ecosystem` and shallow-clones `nexigen-cpp`:

```yaml
- name: Clone nexigen-cpp sibling dependency
  run: |
    mkdir -p ../nexigen-ecosystem
    git clone --depth 1 https://github.com/nexigen-tech/nexigen-cpp.git ../nexigen-ecosystem/nexigen-cpp
```

A shallow clone keeps the step fast. The default branch of `nexigen-cpp` is assumed to be compatible; if compatibility breaks in the future, pin to a specific ref.

## Artifact improvement

The original upload step warned when `Testing/Temporary/LastTest.log` did not exist (e.g., configure or build failed before CTest ran). Update the step to:
- Include `CMakeOutput.log` and `CMakeError.log`.
- Use `if-no-files-found: ignore` to suppress warnings when logs are absent.
- Rename the artifact from `test-logs-*` to `logs-*` to reflect the broader content.
