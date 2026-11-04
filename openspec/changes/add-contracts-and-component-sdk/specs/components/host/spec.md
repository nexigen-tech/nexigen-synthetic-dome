# Spec Delta

## Purpose

Provides one language-neutral host and plugin boundary that gives C++ and Rust components identical ownership, lifecycle, compatibility, infrastructure, and hot-path behavior.

## ADDED Requirements

### Requirement: C-compatible plugin boundary

The plugin boundary SHALL use only fixed-layout C-compatible values, opaque handles, byte buffers, and status codes; C++ library types and Rust-owned standard types SHALL NOT cross it.

#### Scenario: C++ and Rust export the same ABI

- **WHEN** equivalent C++ and Rust plugins are inspected by the host
- **THEN** both expose the same ABI major, structure sizes, lifecycle entry points, and fixed-layout frame context

### Requirement: Plugin SDK and contract validation

The host SHALL validate plugin SDK ABI, component identity, contract versions, `layout_hash` values, and required resources before component initialization.

#### Scenario: ABI mismatch prevents initialization

- **WHEN** a plugin reports an unsupported ABI major or incompatible contract hash
- **THEN** the host rejects it before calling its initialization entry point and releases any loaded plugin resources

### Requirement: Explicit cold-path ownership

Each plugin SHALL create and destroy its own component state through paired entry points, while the host retains ownership of frame contexts and input/output buffers.

#### Scenario: Borrowed output cannot outlive a step

- **WHEN** a component attempts to retain a host-provided input or output reference after its step returns
- **THEN** the component violates the documented lifetime contract, and SDK tests use guarded buffers to detect such retention during development

### Requirement: Ordered lifecycle and rollback

The host SHALL execute initialization, arming, stepping, and stopping in order and SHALL roll back completed stages in reverse order when any stage fails.

#### Scenario: Arm failure prevents stepping

- **WHEN** initialization succeeds but arming fails
- **THEN** the host never calls step, invokes the applicable cleanup path, shuts down Infrastructure, and returns the original failure

#### Scenario: Stop is idempotent after failure

- **WHEN** rollback or shutdown is requested more than once
- **THEN** component and Infrastructure resources are released at most once and no additional frame is stepped

### Requirement: Shared nexigen Infrastructure

The host SHALL own one nexigen Infrastructure per process, initialize it before plugin execution, and stop the component before flushing and destroying Infrastructure.

#### Scenario: Final component telemetry is preserved

- **WHEN** a component stops normally or through the lifecycle failure path
- **THEN** component work stops before tracer, metrics, chrono, and logger flush/shutdown completes

### Requirement: Thread specifications are validated and applied

The host SHALL validate declared thread specifications and apply them on the target thread before work begins, preserving required-versus-best-effort behavior.

#### Scenario: Required FIFO configuration fails

- **WHEN** the operating system rejects a required scheduling or affinity setting
- **THEN** the host aborts arming, reports the thread application failure, and does not silently run the component with reduced settings

### Requirement: Hot path has no implicit resource operations

After successful arming, host-mediated frame stepping SHALL perform no heap allocation, lock, formatting, JSON processing, thread creation, or plugin loading.

#### Scenario: Empty component has a bounded ABI call

- **WHEN** an initialized empty component is stepped repeatedly
- **THEN** each frame crosses the plugin boundary through the fixed lifecycle call without additional allocation or synchronization

### Requirement: Frame context excludes host-specific facilities

The component frame context SHALL expose only logical frame/time values, seeded randomness, typed ports, and approved telemetry handles, not wall-clock, physical channels, CPU settings, or transport endpoints.

#### Scenario: Component cannot depend on physical deployment through context

- **WHEN** the same component is driven with two different source and thread configurations
- **THEN** its frame context differs only in logical frame data and approved handles, not in hardware or placement identifiers

### Requirement: Language failures cannot unwind across the boundary

C++ exceptions and Rust panics SHALL NOT propagate across the plugin boundary; Rust component builds SHALL use abort-on-panic behavior.

#### Scenario: Rust plugin panics during a test

- **WHEN** a test Rust plugin panics while called by the host
- **THEN** the child test process terminates without unwinding into C++ frames, and the test reports the abnormal termination rather than continuing with corrupted component state

### Requirement: Component step failures are explicit

A component SHALL return a fixed status code from step, and the host SHALL stop further stepping and report the failure without allocating or formatting on the hot path.

#### Scenario: Step returns an error

- **WHEN** a component returns a failure status for frame N
- **THEN** the host records the status and frame number, does not execute frame N+1, and enters the defined stop path
