# Design

## Context

The project already has C++ ABI-1 plugins for CAN (VirtualCan, CanMonitor) and in-process iceoryx2 transport tests, but lacks a standalone Rust integration example that external developers can use as a reference for writing their own Rust components with the iceoryx2 transport.

See proposal.md — Why for motivation. Specs at `specs/examples/can-generator/spec.md` and `specs/examples/can-monitor/spec.md` define the observable behaviour.

## Goals / Non-Goals

**Goals:**
- Two standalone Rust binaries that communicate via iceoryx2 shared memory using the `RawCanBatchClassical` contract
- CAN_generator: CLI-driven, headless, publishes CAN frames at 1 ms intervals
- CAN_monitor: GUI-driven, subscribes to a user-selectable CAN bus, displays last 10 frames
- Both binaries added to the workspace so `cargo build --workspace` compiles them
- Minimal new dependencies: reuse `nxd-abi` and `nxd-sdk` for transport; use `clap` for CLI, `egui`/`eframe` for GUI

**Non-Goals:**
- No new contracts or wire format changes
- No changes to the existing C++ host, runtime modules, or SDK
- No changes to the ABI table or plugin loading path
- No deterministic replay or recording integration
- No multi-bus support in a single process

## Decisions

### Decision: egui over iced, slint, or relm for the GUI

egui (immediate-mode) is chosen because it requires zero setup beyond a Cargo dependency, has built-in eframe windowing, and produces a single binary with no system GUI toolkit dependencies. The frame display is simple enough (a scrollable table) that immediate-mode overhead is irrelevant.

Alternatives considered:
- **iced**: Elm-architecture, more idiomatic Rust for complex UIs, but requires async runtime setup and heavier dependency tree for a 10-row table.
- **slint**: Design-tool integration, but adds a code generation step.
- **relm**: Gtk-rs wrapper, requires Gtk system libraries.

### Decision: Direct iceoryx2 C API via nxd-abi instead of the nxd-sdk Component trait

The nxd-sdk Component trait is designed for ABI-1 plugins loaded by the nxd-host. The CAN_generator and CAN_monitor are standalone processes with no ABI-1 entry point — they open the iceoryx2 transport directly. Using `nxd-abi` types (`RawCanBatchClassical`, `NxdSimTime`) is sufficient; wrapping with `nxd-sdk` adds the Component lifecycle which is unnecessary here.

The Rust SDK's `nxd_abi.h` (cbindgen-generated) exposes `iox2_node_h` and `iox2_publisher_h` types via the `IceoryxTransport` C++ class, but that class is C++ only. For Rust, we use the iceoryx2 Rust crate directly via `nxd-abi` — the `RawCanBatchClassical` struct is `#[repr(C)]` and usable directly from Rust.

### Decision: clap for CLI argument parsing

clap is the de facto standard for Rust CLI argument parsing. For a single argument (`--can-bus`), it adds negligible compile time.

### Decision: Rust `std::sync::mpsc` channel for GUI thread safety

The iceoryx2 subscriber callback runs on a separate receive thread. Decoded CAN frames are sent to the GUI thread via a bounded `std::sync::mpsc::channel` with capacity 32. The GUI polls this channel at its refresh rate (60 Hz) and drains available frames into the 10-entry ring buffer.

## Risks / Trade-offs

- **GUI thread blocking** → The subscriber runs on its own thread; the GUI thread never blocks on iceoryx2 take. The channel decouples the hot receive path from the UI frame rate.
- **egui refresh rate** → egui redraws on demand (input events) and at a configurable max FPS. CAN frames arriving between redraws are buffered in the channel and batch-displayed on the next redraw — worst-case latency is ~16 ms at 60 FPS, acceptable for a monitoring display.
- **iceoryx2 service name collision** → Both generator and monitor must agree on the bus name. Using the same default (`CAN_DEMO`) for both avoids confusion; overriding via CLI (generator) or GUI (monitor) allows isolation.
- **No hot-swap for generator bus name** → The generator takes the bus name at startup and cannot change it at runtime. This is acceptable — the generator is a headless source, not an interactive tool.
