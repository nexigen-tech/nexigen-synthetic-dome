# Spec Delta

## Purpose

Provides a stable, bounded video stream and frame contract that transports diverse raw, encoded, and vendor-specific video without making transport or recording understand every codec.

## ADDED Requirements

### Requirement: Versioned video stream description

Each video stream SHALL have a versioned descriptor containing stream identity, revision, coding, maximum frame size, and bounded format-specific configuration.

#### Scenario: Receiver validates stream compatibility

- **WHEN** a receiver supports the contract but not the descriptor's coding or revision
- **THEN** it reports an unsupported stream before processing frames rather than misinterpreting payload bytes

### Requirement: Raw and encoded codec coverage

The contract SHALL represent Raw, JPEG, H.264, H.265, AV1, H.266/VVC, `DeviceRaw`, and extensible other codecs without changing the common frame layout.

#### Scenario: H.266 access unit remains opaque

- **WHEN** an H.266 access unit is transported to a recorder without an H.266 decoder
- **THEN** its bytes and metadata remain intact and the receiver reports only that display decoding is unsupported

### Requirement: Raw pixel and plane description

For supported raw formats, the descriptor SHALL describe pixel format, dimensions, rational frame rate, and bounded plane layout, including stride and offsets.

#### Scenario: Multi-planar frame retains layout

- **WHEN** a supported multi-planar raw frame is produced
- **THEN** its plane count and each plane's offset, stride, and byte count are available without pointers into process-local memory

### Requirement: DeviceRaw opaque video frames

`DeviceRaw` SHALL represent one vendor-defined video frame as opaque bounded bytes with vendor format identity and optional schema metadata in the stream descriptor.

#### Scenario: DeviceRaw needs no generic pixel interpretation

- **WHEN** a vendor library produces a DeviceRaw frame whose bytes describe one video frame
- **THEN** transport and recording preserve the frame without requiring width, height, pixel format, planes, or a separate raw-DMA contract

### Requirement: Frame identity and bounded payload

Each video frame SHALL contain its stream revision, content frame index, actual payload byte count, kind (`Data` or `Heartbeat`), and status (`Valid`, `Degraded`, or `Invalid`).

#### Scenario: Payload exceeds bounded storage

- **WHEN** a producer attempts to publish more bytes than the descriptor's maximum frame size
- **THEN** publication fails with a capacity error and no out-of-bounds write occurs

### Requirement: Aggregate video error reporting

Video frames SHALL report aggregate error flags and a vendor error code, including missing lines, CRC mismatch, packet loss, truncation, timing, and hardware failures, without per-line error regions.

#### Scenario: Missing lines are summarized

- **WHEN** hardware detects one or more missing video lines and a CRC mismatch
- **THEN** the frame reports `Degraded` or `Invalid` with `MissingLines` and `CrcMismatch` flags and no per-line detail is required

### Requirement: Heartbeat has independent hardware timing

A video heartbeat SHALL carry its hardware formation timestamp and SHALL NOT increment the content frame index or require video decoding.

#### Scenario: Heartbeat indicates no signal

- **WHEN** a receiver obtains a heartbeat after a data frame
- **THEN** it can derive the hardware-timed inactivity interval, update health to no-signal as configured, and leave the content frame index unchanged

### Requirement: Transport sequence and content sequence are independent

The common sample sequence SHALL advance for every video sample, including heartbeat, while the content frame index SHALL advance only for data frames.

#### Scenario: Heartbeat does not create a content gap

- **WHEN** one or more heartbeats occur between two valid data frames
- **THEN** transport sequence advances through the heartbeats while consecutive data content indices remain consecutive

### Requirement: Stream revision changes are explicit

A change in coding, dimensions, pixel format, or codec configuration SHALL produce a new stream revision that receivers can detect before applying the new format.

#### Scenario: Resolution changes during a stream

- **WHEN** a source changes video resolution
- **THEN** subsequent frames carry the new stream revision and the receiver revalidates its decoder or converter before displaying them
