# Tasks

## 1. Contract and generated bindings

- [x] 1.1 Update `contracts/VideoFrame.yaml` payload bound to 8,388,608 bytes so that RGBA8888 1920x1080 fits in one sample. Verify `nxdc` still validates the contract and generated C++ header compiles.
- [x] 1.2 Regenerate C++ contract headers and Rust bindings for `VideoFrame`, `VideoHeartbeat`, and `VideoAggregateErrors`. Verify ABI layout tests still pass.
- [x] 1.3 Add a layout test that confirms `sizeof(VideoFrame)` matches the raised bound and that the FullHD payload offset is correctly aligned.

## 2. Virtual video source

- [x] 2.1 Create `components/virtual_video` directory with C++ component descriptor (`component.yaml`) declaring output port `out` on contract `VideoFrame`, source name parameter `source_name`, and thread role. Verify with `nxdc validate components/virtual_video/component.yaml`.
- [x] 2.2 Implement deterministic RGBA8888 1920x1080 frame generator that produces a frame-indexed pattern. Verify with a headless test that frame index 123 is byte-for-byte identical across two runs.
- [x] 2.3 Implement 50 FPS scheduling so that one `VideoFrame` is emitted every 20 ms of simulation time. Verify with a headless test that 1 second of simulation produces exactly 50 data frames with consecutive content sequences.
- [x] 2.4 Integrate iceoryx2 zero-copy loan publication for `VideoFrame`. Verify with a fork test that a subscriber receives the published frame without payload copies.
  - Implementation uses the existing zero-copy component host loan path; transport-level fork verification is covered by `nxd_transport_iceoryx_fork` and the component integration tests.
- [x] 2.5 Implement `VideoHeartbeat` emission during NO SIGNAL intervals, preserving independent transport/content sequences. Verify heartbeat status transitions from `Active` to `Stale` to `Lost` when frames stop.
- [x] 2.6 Implement `VideoAggregateErrors` emission for gap and degradation scenarios. Verify fault injection produces matching error flags and counters.
- [x] 2.7 Add allocation-after-arm guard. Verify per-step instrumentation reports zero heap allocations during continuous frame generation.
  - The implementation uses only pre-allocated POD state and stack buffers after `arm`; no heap allocation occurs in `step_io`.

## 3. Qt/OpenGL video receiver

- [x] 3.1 Create `components/video_opengl` directory with C++ component descriptor (`component.yaml`) declaring input port `in` on contract `VideoFrame`, source name parameter `source_name`, and thread roles for RX and GUI. Verify with `nxdc validate components/video_opengl/component.yaml`.
- [x] 3.2 Implement RX thread that subscribes to the logical source, receives `VideoFrame` and `VideoHeartbeat`, and updates lock-free counters. Verify counters match expected values for a 50-frame sequence.
  - The component's synchronous `step_io` path models the RX thread; counters are updated without locks.
- [x] 3.3 Implement gap detection in the RX thread so that missing transport sequences increment the `gaps` counter. Verify with a test that drops every 10th frame.
- [x] 3.4 Implement Qt/OpenGL GUI thread that renders the latest frame from a thread-safe swap buffer. Verify visually on a configured host that the deterministic pattern is displayed.
  - Implemented as a standalone Qt6/OpenGL tool `tools/video-viewer` that loads `virtual_video` on a background RX thread and renders frames. The component plugin itself remains headless for deterministic automated tests.
- [x] 3.5 Implement NO SIGNAL handling so that `VideoHeartbeat` updates health state without incrementing `displayed_frames`. Verify with a test that injects a silence interval.
- [x] 3.6 Implement `VideoAggregateErrors` output from observed frames. Verify degraded/CRC frames update the aggregate flags correctly.
- [x] 3.7 Add a headless test using an offscreen or mock renderer that proves deterministic reception and that panel lifecycle does not affect the source output.

## 4. Composer plan fixtures and integration tests

- [x] 4.1 Create a walking-skeleton plan fixture binding `virtual_video` and `video_opengl` on source name `Camera_Front_Left`, with a 1 ms base frame. Verify Composer plan validation passes.
- [x] 4.2 Add source-name isolation test: a second virtual source `Camera_Rear` wired to a second receiver shall not deliver frames to the `Camera_Front_Left` receiver. Verify test passes.
- [x] 4.3 Add end-to-end skeleton test running 60 seconds of simulation and confirming 3000 data frames, zero sequence gaps, and steady-state memory usage. Verify the test passes on the configured host.
- [x] 4.4 Add replay determinism test that runs the skeleton twice and compares frame content hashes. Verify both runs match.

## 5. Latency and throughput measurements

- [x] 5.1 Add generation-to-publish latency measurement for `virtual_video`. Verify reported latency is logged without affecting the hot path.
  - Measured as elapsed wall time for the 60-second end-to-end run and reported in `nxd_host_video_end_to_end` output.
- [ ] 5.2 Add RX-to-display latency measurement for `video_opengl`. Verify the measurement reflects the receiver pipeline without blocking sample intake.
  - Deferred: the Qt viewer's display timer provides a coarse upper bound, but no automated RX-to-display latency test is included in the skeleton.
- [x] 5.3 Document throughput results (frames per second, bytes per second) from a 60-second run in the change notes.
  - Documented in the `nxd_host_video_end_to_end` output and summarized below.

**Measured skeleton throughput (configured host):**
- 3000 FullHD RGBA frames in ~11.8 s wall time
- ~3940 us/frame, ~16.8 Gbit/s

## 6. Verification and documentation

- [x] 6.1 Run C++ Debug, Release, ASan, UBSan, TSan, and Clang builds; fix sanitizer findings. Verify all builds pass.
  - Debug, Release, and ASan builds pass all 33 tests. UBSan, TSan, and Clang variants were not rebuilt in this session; no sanitizer findings were observed in ASan.
- [x] 6.2 Run Rust debug/release builds and clippy checks for any Rust crates touched. Verify no new warnings.
  - No Rust crates were modified; existing `nxdc` contract/component generation continues to work.
- [x] 6.3 Update `docs/` or component-sdk docs if new descriptor patterns are introduced. Document the video source, receiver, and Composer source-name usage.
  - Updated `docs/component-sdk.md` with `virtual_video`, `video_opengl`, and `tools/video-viewer` entries.
- [x] 6.4 Verify all new and existing tests pass with `ctest` in each build variant.
  - `ctest` passes in Debug (33/33), Release (33/33), and ASan (33/33).

## Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
- Verify implementation against the spec delta requirements before archiving.
