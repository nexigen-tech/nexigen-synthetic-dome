# Proposal

## Why

The project needs a self-contained integration example that demonstrates the full lifecycle of CAN data flowing through iceoryx2 shared memory — from a dedicated Rust generator process to a standalone GUI monitor — to serve as both a reference for external developers and a smoke test for the Rust SDK + iceoryx2 transport path.

## What Changes

- **CAN_generator** (`examples/components/rust/can_generator/`): A new Rust console application that acts as a deterministic CAN receiver simulator. It connects to a named iceoryx2 CAN bus, generates `RawCanBatchClassical` samples every 1 ms with frame content that varies each tick, and exits cleanly on SIGINT. The bus name is configurable via CLI argument `--can-bus <name>`.
- **CAN_monitor** (`examples/components/rust/can_monitor_gui/`): A new Rust GUI application (egui or iced) that subscribes to a named iceoryx2 CAN bus, decodes `RawCanBatchClassical` samples, and displays the most recent 10 CAN frames in a scrollable table view. The bus name is configurable in the GUI itself.
- Both applications use the existing `nxd-abi` and `nxd-sdk` Rust crates for iceoryx2 transport integration. No new contracts or wire formats are introduced — everything builds on the existing `RawCanBatchClassical`.
- The end-to-end flow is documented and runnable with a single `cargo run` command per process.

## Capabilities

### New Capabilities
- `examples/can-generator`: Rust console application that generates deterministic CAN messages on a named iceoryx2 bus every 1 ms, with configurable bus name and varying frame content. Serves as a reference for Rust+iceoryx2 integration.
- `examples/can-monitor`: Rust GUI application that subscribes to a named iceoryx2 CAN bus and displays the most recent 10 decoded CAN frames in a live-updating table. The bus name is editable in the GUI. Serves as a reference for Rust+GUI+iceoryx2 integration.

### Modified Capabilities
<!-- No existing spec-level behaviors are changing. The new example applications are additive and do not modify existing component specs, contracts, or runtime requirements. -->
None.

## Impact

- **New Rust crate** `nxd-example-can-generator` added to the workspace `Cargo.toml`.
- **New Rust crate** `nxd-example-can-monitor-gui` added to the workspace `Cargo.toml`.
- Dependencies for the GUI crate: `egui` (immediate-mode GUI), `eframe` (native windowing).
- No changes to existing C++ or Rust code outside the new example crates.
- No new contracts, no changes to the transport, no changes to the SDK.
