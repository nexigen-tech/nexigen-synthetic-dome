# Virtual Video Source Specification

## Purpose

Provides a deterministic, hardware-faithful virtual video logical source that generates FullHD RGBA frames for synthetic environments without requiring physical cameras or video codecs.

## Requirements

### Requirement: Virtual source publishes one VideoFrame per frame interval
The virtual video source SHALL emit exactly one `VideoFrame` sample per configured frame interval on its assigned source name, advancing the transport sequence for every sample and the content sequence only for data frames.

#### Scenario: 50 FPS produces one frame every 20 ms
- **WHEN** the source is configured for 50 FPS and stepped for one second
- **THEN** it emits 50 `VideoFrame` samples with monotonically increasing transport sequences

#### Scenario: Heartbeat does not increment content sequence
- **WHEN** a `VideoHeartbeat` is emitted between two data frames
- **THEN** the transport sequence advances through the heartbeat while the content sequence of the next data frame is consecutive to the previous data frame

### Requirement: Virtual source generates deterministic RGBA8888 FullHD frames
The virtual source SHALL produce RGBA8888 frames at 1920x1080 resolution with a deterministic, frame-indexed dynamic pattern that is repeatable across runs.

#### Scenario: Same frame index produces identical pixels
- **WHEN** the source generates frame index 123 twice with the same configuration
- **THEN** both frames contain identical pixel bytes

#### Scenario: Frame metadata matches the configured stream
- **WHEN** the source emits a data frame
- **THEN** the `VideoFrame` carries width 1920, height 1080, codec `Raw`, and frame rate 50/1

### Requirement: Frame pixels are published through a zero-copy loan
The virtual source SHALL write generated pixels directly into an iceoryx2 zero-copy loan bound to the `VideoFrame` sample; the payload SHALL fit within the bounded fixed storage declared for the skeleton stream.

#### Scenario: FullHD RGBA frame fits in the loan
- **WHEN** the source generates one FullHD RGBA frame
- **THEN** the published `VideoFrame` payload contains exactly 8,294,400 bytes and no separate heap allocation is made

### Requirement: Virtual source emits VideoHeartbeat during NO SIGNAL
When no data frame is produced for a configured interval, the source SHALL emit a `VideoHeartbeat` sample with status `Stale` or `Lost`, preserving the last content sequence and an independent hardware formation timestamp.

#### Scenario: Silence produces heartbeat
- **WHEN** no data frame is produced for 40 ms
- **THEN** the source emits a `VideoHeartbeat` with status `Stale` and the content sequence unchanged

### Requirement: Virtual source reports VideoAggregateErrors for reception anomalies
The source SHALL publish a `VideoAggregateErrors` sample once per base frame, reporting gaps, losses, and degradation flags observed in the generated stream.

#### Scenario: Missing frame increments gap counter
- **WHEN** one expected data frame is skipped due to a configured fault injection
- **THEN** the emitted `VideoAggregateErrors` reports one gap and the `MissingLines` or error flags reflect the injected condition

### Requirement: Source-name wiring isolates independent video streams
The virtual source SHALL bind to a declared logical source name; unrelated source names SHALL NOT receive its frames.

#### Scenario: Camera_Front_Left and Camera_Front_Right are wired separately
- **WHEN** a receiver subscribes to source name `Camera_Front_Right`
- **THEN** it never receives frames published on `Camera_Front_Left`

### Requirement: Hot path performs no heap allocation
After successful arming, the virtual video source SHALL not allocate heap memory, acquire locks, parse configuration, or create threads while producing frames.

#### Scenario: Continuous frame generation is allocation-free
- **WHEN** the source is armed and stepped repeatedly
- **THEN** per-step instrumentation reports zero heap allocations
