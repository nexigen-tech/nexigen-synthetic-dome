# Spec Delta

## Purpose

Provides a Qt/OpenGL video receiver that subscribes to a logical video source, displays the latest frame, and exposes reception counters without affecting deterministic simulation execution.

## ADDED Requirements

### Requirement: Receiver subscribes by logical source name

A video receiver SHALL declare an input port bound by logical source name and SHALL receive only frames published on that source name.

#### Scenario: Front-left receiver ignores a different camera

- **WHEN** a receiver subscribes to source name `Camera_Front_Left`
- **THEN** frames published on `Camera_Front_Right` do not appear at its input port

#### Scenario: Same receiver reused for a different source

- **WHEN** a second receiver instance is configured with source name `Camera_Rear`
- **THEN** it receives only frames published on `Camera_Rear`

### Requirement: Receiver separates RX and GUI threads

The receiver SHALL process incoming samples on a dedicated RX thread and render frames on a GUI thread, avoiding GUI work on the simulation hot path.

#### Scenario: Long GUI frame does not block sample intake

- **WHEN** the GUI thread stalls briefly while the RX thread continues receiving samples
- **THEN** no samples are dropped due to the GUI stall and the latest frame is displayed once the GUI thread resumes

### Requirement: Receiver tracks reception counters

The receiver SHALL maintain counters for `rx_samples`, `rx_data_frames`, `displayed_frames`, `gaps`, and `errors`, updating them atomically without locks on the hot path.

#### Scenario: 50 frames received and 48 displayed

- **WHEN** 50 data frames and 2 heartbeats arrive and 2 frames are skipped by display decimation
- **THEN** `rx_samples == 52`, `rx_data_frames == 50`, `displayed_frames == 48`, and `gaps == 0`

#### Scenario: Gap detection increments gap counter

- **WHEN** a missing transport sequence is detected between two received frames
- **THEN** the `gaps` counter is incremented by the number of missing frames

### Requirement: Receiver handles VideoHeartbeat NO SIGNAL

The receiver SHALL interpret `VideoHeartbeat` samples as no-signal indicators and update its health state without incrementing the displayed frame counter.

#### Scenario: Heartbeat after data frame marks no signal

- **WHEN** a `VideoHeartbeat` with status `Lost` arrives after a sequence of data frames
- **THEN** the receiver reports no signal and does not count the heartbeat as a displayed frame

### Requirement: Receiver displays the latest frame via OpenGL

The receiver SHALL upload the most recent `VideoFrame` payload to an OpenGL texture and render it in a Qt window at the display refresh rate.

#### Scenario: Newer frame replaces pending frame

- **WHEN** two data frames arrive during one display interval
- **THEN** the receiver displays the newer frame and accounts for both in `rx_data_frames`

### Requirement: Receiver output does not affect deterministic simulation

Closing the receiver panel, changing its window size, or restarting it SHALL NOT alter the sequence of frames produced by the virtual source or the simulation result.

#### Scenario: Panel restart preserves source determinism

- **WHEN** the receiver panel is closed mid-run and reopened
- **THEN** the virtual source continues to emit the same frame sequence and a repeated run without panel restart produces identical output

### Requirement: Receiver reports aggregate errors from observed frames

The receiver SHALL emit a `VideoAggregateErrors` sample once per base frame, reflecting gaps, CRC errors, missing lines, and other flags observed in received frames.

#### Scenario: Degraded frame increments degraded counter

- **WHEN** a received frame has error flags set
- **THEN** the emitted `VideoAggregateErrors` reports the corresponding flags and the error counter is incremented
