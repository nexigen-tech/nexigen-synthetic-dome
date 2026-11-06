# Proposal

## Why

The project already has bounded video contracts, a language-neutral component host, the Chronos runtime, and a working virtual CAN source. The next step is to prove that high-bandwidth sensor streams can be generated, transported, and displayed deterministically inside the synthetic environment. A FullHD RGBA virtual camera source and a minimal Qt/OpenGL receiver provide a self-contained, repeatable example that exercises the zero-copy transport, source-name subscriptions, partition/LET boundaries, and monitor panels without requiring real camera hardware or video codecs.

## What Changes

- Add a deterministic `virtual_video` logical source that publishes `VideoFrame` samples on source name `Camera_Front_Left` at 50 FPS, using RGBA8888 1920x1080 frames with a deterministic, frame-indexed dynamic pattern.
- Generate frame pixels directly into an iceoryx2 zero-copy loan; use bounded fixed storage sized for the skeleton configuration.
- Emit `VideoHeartbeat` samples to represent `NO SIGNAL` conditions and preserve independent transport/content sequences.
- Report aggregate video errors via `VideoAggregateErrors` for gap, loss, and degradation scenarios.
- Add a Qt/OpenGL video receiver that subscribes to `Camera_Front_Left` by source name, splits RX and GUI threads, and tracks `rx_samples`, `rx_data_frames`, `displayed_frames`, gaps, and errors.
- Add headless component tests and a walking-skeleton scenario that prove deterministic frame generation and reception.

**Out of scope (v1):** H.264/H.265/AV1/H.266 decoding, all raw pixel backends, RTP reassembly, `DeviceRaw` source, external DMA buffers, and a dedicated `raw_dma_source`. Generic panel UI beyond the Qt/OpenGL receiver is also deferred.

## Capabilities

### New Capabilities

- `sources/virtual-video`: Logical video source `Camera_Front_Left`; deterministic RGBA8888 FullHD 50 FPS frame generation; direct iceoryx2 loan publication; bounded fixed storage; `VideoHeartbeat` and `VideoAggregateErrors` emission; source-name parameter.
- `receivers/video-opengl`: Qt/OpenGL video receiver; source-name subscription; separate RX and GUI threads; counters for received samples, data frames, displayed frames, gaps, and errors; `NO SIGNAL` heartbeat handling.

### Modified Capabilities

- None for v1. Existing `contracts/video` requirements and component descriptor parameters are reused as-is. The concrete `VideoFrame.yaml` bound will be adjusted if necessary to fit FullHD RGBA payload without changing spec-level behavior.

## Impact

- **New systems:** `sources/virtual-video` component, `receivers/video-opengl` component and panel, walking-skeleton scenario and tests.
- **Build:** new C++ targets for the virtual source and Qt receiver; new component descriptors and generated bindings; new test fixtures.
- **Dependencies:** `contracts/video` (`VideoFrame`, `VideoHeartbeat`, `VideoAggregateErrors`), `runtime/transport` zero-copy loans, `runtime/chronos` frame distribution, Composer source-name wiring, component host lifecycle.
- **Compatibility:** no breaking changes to existing contracts or host ABI; new components declare new logical ports and parameters.
- **Hot-path rules:** frame generation and publication use zero heap allocation after `Arm`; pixel buffer allocation and Qt initialization happen during initialization.
- **Follow-on changes:** encoded codec pipelines, DeviceRaw source, external DMA buffers, recorder/replay of video streams, Tauri-based panels.
