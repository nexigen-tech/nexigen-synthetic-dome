# Design

## Context

The previous changes established bounded video contracts, the component host, the Chronos runtime, Composer validation, the typed port I/O transport, and a language-neutral plugin boundary. The repository therefore already provides:

- `VideoFrame`, `VideoHeartbeat`, and `VideoAggregateErrors` contracts;
- component descriptor schema with file-valued parameters, thread roles, and logical ports;
- `runtime/chronos` frame context and partition/LET semantics;
- `runtime/composer` manifest validation, including source-name wiring from the virtual CAN change;
- `runtime/transport` iceoryx2 zero-copy loan support;
- `host/` C ABI for C++ and Rust plugins.

This change layers a virtual video source and a Qt/OpenGL receiver on top of that foundation without changing contracts at the spec level or the host ABI. See `proposal.md` for motivation.

## Goals / Non-Goals

**Goals:**

- Deliver a deterministic virtual video logical source (`Camera_Front_Left`) that publishes `VideoFrame` samples at 50 FPS.
- Provide a `video_opengl` receiver that subscribes to `Camera_Front_Left`, displays frames, and publishes reception counters.
- Add walking-skeleton tests that prove deterministic frame generation, zero-allocation hot path, and source-name isolation.

**Non-Goals / Deferred:**

- Encoded codec decoding (H.264/H.265/AV1/H.266), JPEG pipelines, RTP reassembly, and `DeviceRaw` handling.
- External DMA buffers and a dedicated `raw_dma_source`.
- Generic panel UI beyond the Qt/OpenGL receiver.
- Tauri-based panels; this change uses Qt/OpenGL as decided in D-24.
- Multi-source FullHD production; only one FullHD stream is exercised in the main skeleton, with a second source used only in small binding tests.

## Decisions

### D1. Virtual video source is a stateless deterministic producer component

`virtual_video` is implemented as a component with one output port carrying `VideoFrame`. It owns a pre-allocated pixel buffer created during initialization. On each step it:

- receives the current simulation frame and `SimTime` from Chronos;
- determines whether a video data frame is due based on the configured 50 FPS interval;
- generates an RGBA8888 1920x1080 pixel pattern deterministic in the content frame index;
- writes the pixels into an iceoryx2 zero-copy loan bound to the `VideoFrame` sample;
- emits the `VideoFrame` with updated transport and content sequences;
- emits a `VideoHeartbeat` when no data frame is due and a silence threshold is crossed;
- emits a `VideoAggregateErrors` sample reflecting any injected or detected anomalies.

This keeps the source independent of physical cameras and lets the same component run in headless tests, SIL, or step mode.

### D2. VideoFrame payload bound is raised for FullHD RGBA skeleton

The existing `contracts/VideoFrame.yaml` declares a 2048-byte `payload` array, which is insufficient for RGBA8888 1920x1080 (~8.3 MiB). For the skeleton, the bound is raised to 8,388,608 bytes so that a single `VideoFrame` sample can carry one FullHD raw frame inside the iceoryx2 loan. This is a concrete contract bound adjustment, not a spec-level behavior change; future changes may introduce parametric size classes (similar to `RawCanBatchClassical`/`Fd`/`Xl`) when multiple raw resolutions or encoded codecs are supported.

**Alternative considered:** introduce a parametric `VideoFrame<MaxPayload>` contract now. Rejected for v1 because it expands the IDL/compiler scope beyond the skeleton and the durable `contracts/video` spec does not yet require size-class parameterization.

### D3. Frame generation is deterministic and index-based

Pixel content is derived from the content frame index using a simple deterministic function (for example, a rolling color gradient plus a frame counter overlay). No RNG or wall-clock is used after initialization, so repeated runs with the same plan produce identical pixel bytes.

### D4. Heartbeat preserves independent sequences and HW formation time

`VideoHeartbeat` samples carry their own `transport_sequence` and do not advance `content_sequence`. The source uses the simulation frame time as the hardware formation timestamp (`timestamp_ns`), preserving deterministic replay and satisfying the requirement that heartbeat timing is activity-aware.

### D5. Qt/OpenGL receiver separates RX and GUI threads

The receiver process contains:

- an RX thread that subscribes to the logical source via the component host, receives `VideoFrame` and `VideoHeartbeat` samples, updates lock-free counters, and copies the latest frame payload into a thread-safe swap buffer;
- a GUI thread that runs the Qt event loop, uploads the latest frame from the swap buffer to an OpenGL texture, and renders it at the display refresh rate.

This separation ensures that GUI stalls do not block sample intake and that the receiver does not participate in the simulation hot path.

### D6. Receiver counters are lock-free and hot-path safe

Counters (`rx_samples`, `rx_data_frames`, `displayed_frames`, `gaps`, `errors`) are updated with relaxed atomic operations on the RX thread and read atomically by the GUI thread and any panel snapshot logic. No mutex is held during sample processing.

### D7. Source-name subscription is enforced by existing Composer partition wiring

Composer already supports an optional `source_name` on `Partition` and a `validate_source_name_wiring` rule from the virtual CAN change. The video source and receiver reuse this mechanism: instances in a partition tagged `Camera_Front_Left` can only wire to other instances in the same source-name partition. This isolates logical video streams at plan-validation time without runtime filtering.

### D8. Walking skeleton reuses existing Composer/Chronos plumbing

The skeleton fixture declares:

- one partition/source `Camera_Front_Left` containing `virtual_video` and `video_opengl`;
- a LET boundary or direct wire from `virtual_video:out` to `video_opengl:in`;
- a 1 ms base frame interval and 50 FPS video frame rate.

No new runner is introduced; tests use the existing headless harness and Composer plan fixtures.

## Risks / Trade-offs

- **Raising `VideoFrame` payload to 8 MiB increases per-sample memory footprint** → mitigated by using one active loan at a time and bounded subscriber queues; future size classes will reduce memory for encoded streams.
- **Qt/OpenGL receiver is platform-dependent and requires a display or offscreen surface** → headless tests use an offscreen platform plugin or mock renderer; the GUI path is verified manually on a configured host.
- **FullHD 50 FPS stresses the zero-copy transport** → the skeleton includes latency and throughput measurements to confirm the design before expanding to multiple streams.
- **Receiver panel lifecycle must not affect source determinism** → panel updates are decoupled from the simulation; closing or restarting the panel does not send commands back to the source.

## Migration Plan

1. Update `contracts/VideoFrame.yaml` payload bound to fit FullHD RGBA and regenerate bindings.
2. Add `components/virtual_video` descriptor and implementation.
3. Add `components/video_opengl` descriptor and Qt/OpenGL implementation.
4. Add Composer plan fixtures for `Camera_Front_Left` and source-name isolation (`Camera_Front_Left` vs `Camera_Rear`).
5. Add headless deterministic tests for `virtual_video` and `video_opengl`, plus replay checks.
6. Run C++ Debug/Release/ASan/TSan/UBSan/Clang and Rust debug/release suites.

Rollback removes the new component directories and reverts the `VideoFrame.yaml` bound without affecting host ABI or existing Chronos/Composer targets.

## Open Questions

- Whether the Qt/OpenGL receiver tests will use an offscreen platform plugin or a headless mock renderer will be decided during implementation based on CI environment capabilities.
- The exact deterministic pixel generation function (gradient pattern, overlay content) will be fixed in tasks to ensure it is cheap to compute and visually recognizable.
