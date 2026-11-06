# Proposal: Stabilize walking skeleton across build variants

## Problem

The walking skeleton passed in the default and debug CMake configurations, but several other build variants were broken:

1. **Release build:** `nxd_nexigen_run_services` failed because `tools/nexigen-run/tests/fixtures/nxd-ws01-services.json` and the generated `replay.mcap` were not copied into the build tree unless the fixture-generator target happened to be built.
2. **Clang build:** Failed with `-Werror,-Wunused-const-variable` in the virtual video plugin, and several newer test executables were missing from the stale `build-clang` directory.
3. **Sanitizer builds (ASan/TSan/UBSan):** Many tests failed with `load failed` because generated component descriptors were stale (created before recent contract changes) and CMake did not track contract files as dependencies. Additionally, the blackboard seqlock used `std::atomic_thread_fence`, which TSan cannot instrument, and the video end-to-end/latency tests took prohibitively long under instrumentation.

## Goal

Make `default`, `debug`, `release`, `asan`, `tsan`, `ubsan`, and `clang` build variants pass their relevant test suites without regressions in the already-green configurations.

## Scope

In scope:
- Fix the fixture-copy dependency in `tools/nexigen-run/CMakeLists.txt`.
- Fix the Clang unused-constant warning in `examples/components/cpp/virtual_video/virtual_video.cpp`.
- Add contract-file dependencies to the descriptor-generation custom command in `CMakeLists.txt`.
- Regenerate stale descriptors and rebuild plugins in the affected build directories.
- Suppress the TSan `atomic_thread_fence` warning in `runtime/blackboard/CMakeLists.txt`.
- Disable `nxd_host_video_end_to_end` and `nxd_video_viewer_latency` for sanitizer builds.
- Verify all affected build variants.

Out of scope:
- Rewriting the blackboard seqlock to be fully TSan-compatible (deferred to a follow-up).
- Changing Rust plugin builds; the release plugins work once descriptors match.
- New features or new specs.
- CI setup (will be a follow-up change).

## Success criteria

- `ctest --test-dir build-release --output-on-failure -j4` passes 42/42.
- `ctest --test-dir build-clang --output-on-failure -j4` passes 42/42.
- `ctest --test-dir build-tsan --output-on-failure -j4` passes 40/40 (2 disabled performance tests).
- `ctest --test-dir build-ubsan --output-on-failure -j4` passes 40/40 (2 disabled).
- `ctest --test-dir build-asan --output-on-failure -j4` passes 40/40 (2 disabled).
- `ctest --test-dir build --output-on-failure -j4` continues to pass 42/42.
- `ctest --test-dir build-debug --output-on-failure -j4` continues to pass 42/42.
