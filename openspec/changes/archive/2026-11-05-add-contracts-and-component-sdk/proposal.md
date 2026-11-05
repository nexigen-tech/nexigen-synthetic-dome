# Proposal

## Why

NexDome currently has an agreed architecture but no source-level foundation. A shared, language-neutral contract and component model is needed now so that subsequent runtime, gateway, video, Rust, and panel changes do not develop incompatible binary layouts, lifecycles, or configuration formats.

This change establishes that foundation without prematurely implementing the simulation runtime or any hardware-specific behavior.

## What Changes

- Add a declarative contract IDL with typed aggregates, semantic signal values, bounded payloads, explicit versioning, and deterministic compatibility hashes.
- Generate equivalent C++23 POD and Rust `#[repr(C)]` shared-memory types from one schema, including layout assertions and stable transport type identities.
- Define the common sample metadata and the initial CAN and video contract families, including hardware timestamps, heartbeat, status, and aggregate error reporting.
- Add a declarative component descriptor for ports, parameters, resources, supported modes, thread roles, and component/SDK versions.
- Define a C-compatible plugin ABI and an `nxd-host` lifecycle covering initialization, arming, frame stepping, stopping, and failure rollback for both C++ and Rust components.
- Integrate the host with `nexigen::app::Infrastructure` and `nexigen::core::ThreadSpec` without adding a scheduler or real-time partition runtime.
- Add a headless harness for deterministic frame-by-frame component tests, including C++ and Rust allocation-after-arm detection and Rust panic/FFI policy tests.
- Establish the initial CMake/Cargo build and verification structure for the currently source-free repository.

No existing public NXD API is changed because none exists yet. The initial plugin ABI is versioned explicitly so later incompatible revisions require a new ABI major rather than silent layout changes.

## Capabilities

### New Capabilities

- `contracts/schema`: Declarative schemas, typed aggregates, common sample metadata, C++/Rust POD generation, canonical layout hashing, and cross-language binary compatibility.
- `contracts/can`: Bounded Classical CAN, CAN FD, and CAN XL frame/batch contracts with hardware timestamps, direction, heartbeat, status, and error metadata.
- `contracts/video`: Versioned video stream/frame contracts for raw and encoded video, including AV1, H.266/VVC, `DeviceRaw`, heartbeat, and aggregate error flags.
- `components/descriptor`: Declarative component identity, ports, parameters, thread roles, resources, mode support, and SDK compatibility metadata.
- `components/host`: C-compatible plugin loading, C++/Rust component ownership, host lifecycle, Infrastructure/ThreadSpec integration, and failure handling.
- `components/testing`: Headless frame-driven component testing, allocation-after-arm detection, deterministic lifecycle checks, and Rust panic/FFI verification.

### Modified Capabilities

None. The repository has no existing durable specifications or implementation.

## Impact

- **New systems:** contract schemas and generator, generated C++/Rust libraries, component SDK headers/crates, C plugin ABI, `nxd-host`, examples, and headless test harness.
- **Build:** new root CMake and Cargo workspaces with pinned dependencies; no legacy build behavior is affected.
- **Dependencies:** local `nexigen-cpp` for `Result`/`Status`, Infrastructure, and ThreadSpec; iceoryx2 Rust type-support macros for shared-memory-compatible generated types. Live iceoryx2 transport integration and benchmarks remain outside this change.
- **Compatibility:** one canonical schema is the source of truth. C++ and Rust binaries must use matching contract major versions and `layout_hash` values; unknown or incompatible plugin SDK ABI versions are rejected before component initialization.
- **Hot-path performance:** no IPC or scheduler cost is introduced. The component boundary adds one indirect `step` call per driven frame and fixed-size POD accesses, with no allocation, lock, formatting, exception, or panic propagation. Headless benchmarks must report the measured ABI overhead rather than claim a real-time transport latency.
- **Memory:** all payload and batch capacities are schema-bounded and known before arming. Initialization and schema/JSON processing may allocate on the cold path; steady-state frame stepping must not allocate.
- **Follow-on changes:** Chronos/partitions and minimal Composer, virtual CAN/CANopen, virtual video and Qt receiver, Blackboard/Rust/Tauri integration, and recording/replay build on these artifacts as separate changes.
