# sdk/port-io Specification

## Purpose
Defines typed input/output port views for component step callbacks and the validation rules that ensure zero-allocation, layout-safe data exchange across the plugin boundary.

## Requirements

### Requirement: Step callback receives typed input port views
The SDK SHALL expose input port buffers to the component `step` callback as typed read-only views indexed by port name, where each view is validated against the declared contract `layout_hash`.

#### Scenario: Component reads one RawCanBatch from an input port
- **WHEN** a component declares an input port bound to `RawCanBatchClassical` and the host arms the component
- **THEN** the step callback receives a read-only span of `RawCanBatchClassical` matching the declared contract layout

#### Scenario: Missing input port prevents arming
- **WHEN** a required input port is not bound to any logical source
- **THEN** the host rejects arming with a diagnostic naming the missing port

### Requirement: Step callback receives writable output port loans
The SDK SHALL expose output port buffers to the component `step` callback as writable typed loans that the component fills and returns before the step ends.

#### Scenario: Component publishes one RawCanBatch per step
- **WHEN** a component declares an output port bound to `RawCanBatchClassical`
- **THEN** the step callback can write exactly one sample of that type into the provided loan and publish it without allocation

#### Scenario: Output loan lifetime does not outlive the step
- **WHEN** a component attempts to retain a pointer to an output loan after `step` returns
- **THEN** the SDK detects or prevents the retention during development tests

### Requirement: Port layout is validated against contract hash
The host SHALL compare the port's declared `layout_hash` with the layout hash of the typed view provided to the component and reject mismatches before arming.

#### Scenario: Contract version mismatch is caught
- **WHEN** a component descriptor declares `RawCanBatchClassical` version `1.0.0` but the generated binding in the plugin expects a different layout
- **THEN** arming fails with an ABI mismatch diagnostic

### Requirement: Port I/O performs no allocation after arming
All port views and loans SHALL reference memory allocated during initialization or arming; the step callback SHALL NOT allocate heap memory while accessing ports.

#### Scenario: Stepping with active ports is allocation-free
- **WHEN** a component with input and output ports is armed and stepped repeatedly
- **THEN** allocation instrumentation reports zero heap allocations

### Requirement: Plugin boundary supports both legacy and port-aware step callbacks
The ABI SHALL continue to support plugins that declare no ports and use the existing `step` callback, while also supporting plugins with ports through an extended step callback.

#### Scenario: Legacy empty component still works
- **WHEN** a plugin with no declared ports is loaded
- **THEN** the host calls its legacy `step` callback and does not require port buffer setup

#### Scenario: Port-aware component uses extended callback
- **WHEN** a plugin declares one or more ports
- **THEN** the host uses the extended step callback and validates port bindings before arming
