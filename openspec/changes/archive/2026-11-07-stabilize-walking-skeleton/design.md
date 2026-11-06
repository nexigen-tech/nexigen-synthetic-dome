# Design: Stabilize walking skeleton across build variants

## Release fixture copy

`tools/nexigen-run/CMakeLists.txt` previously copied `nxd-ws01-services.json` only inside a `POST_BUILD` command of `nxd-ws01-replay-fixture-gen`. The services test depended on that target, but if the fixture generator was never built, the JSON file and the generated `replay.mcap` were missing.

Two changes fixed this:
1. Copy the committed `nxd-ws01-services.json` from the `nexigen-run` POST_BUILD command as well, so it is present whenever the runner is built.
2. Add `nxd-ws01-replay-fixture-gen` as a CMake dependency of `nexigen-run`, ensuring the replay fixture is generated in normal builds.

## Clang build

Clang is stricter than GCC about unused constants. `examples/components/cpp/virtual_video/virtual_video.cpp` declared `kFrameNs` but never used it, triggering `-Werror,-Wunused-const-variable`. The constant was removed.

The `build-clang` directory had also been configured before several test executables were added to the project, so those executables were missing. Building the missing targets resolved the "Not Run" failures.

## Sanitizer build descriptor staleness

The root cause of the `load failed` errors in `build-tsan`, `build-ubsan`, `build-clang`, and `build-asan` was stale generated component descriptors. Those build directories were created when the contracts used older names (e.g., `SignalValue` instead of `SignalValueU64`) and lacked `layout_hash`. CMake did not regenerate descriptors because the only tracked dependency was the per-component `component.yaml` file.

The fix has two parts:
1. Add `${NXD_CONTRACT_SOURCES}` to the `DEPENDS` list of every descriptor-generation custom command in `CMakeLists.txt`. Future contract changes now automatically invalidate and regenerate descriptors.
2. Manually delete and regenerate descriptors in the stale build directories, then rebuild the C++ example/test plugins so their hardcoded descriptor hashes match.

No changes to Rust plugin builds were required: the release Rust plugins in `target/release/` load correctly under sanitizers once descriptors match.

## TSan blackboard incompatibility

`runtime/blackboard/src/blackboard.cpp` implements a seqlock using `std::atomic_thread_fence`. TSan cannot instrument this primitive and emits `-Werror=tsan`. As a short-term workaround, `-Wno-tsan` is added to the `nxd_blackboard` target only when `CMAKE_CXX_FLAGS` contains `-fsanitize=thread`.

A future change should rewrite the seqlock with explicit `std::atomic` load/store ordering that TSan can reason about.

## Performance tests under sanitizers

`nxd_host_video_end_to_end` runs 60,001 simulation frames. Under TSan this took over five minutes; under ASan it also would dominate the test run. `nxd_video_viewer_latency` is a GUI/OpenGL latency test that is similarly not meaningful under instrumentation.

CMake now detects sanitizer builds (`CMAKE_CXX_FLAGS` matching `-fsanitize=`) and marks both tests `DISABLED` for those configurations. All other tests still run and pass.

## Verification plan

For each build directory:

1. Reconfigure with CMake to pick up the new dependencies and disabled-test logic.
2. Delete stale generated descriptors and rebuild `nxd_component_descriptors` where needed.
3. Build missing test targets where needed.
4. Run `ctest --output-on-failure -j4`.
5. Confirm pass counts and document disabled tests.
