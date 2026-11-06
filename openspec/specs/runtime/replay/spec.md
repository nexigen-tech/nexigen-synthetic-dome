# runtime/replay Specification

## Purpose
Replays a previously recorded MCAP file as a deterministic logical source, emitting samples on the transport in simulation-time order so that downstream components observe the same sequence as the original run.

## Requirements

### Requirement: Replay reads samples from an MCAP file in simulation-time order

The Replay service SHALL read messages from the MCAP file, sort them by recorded simulation time, and emit each sample on the transport at the corresponding simulation time.

#### Scenario: Samples are emitted at original simulation times
- **WHEN** the recording contains samples at simulation times 0.100 s, 0.200 s, and 0.500 s
- **THEN** the replay emits those samples when Chronos reaches 0.100 s, 0.200 s, and 0.500 s respectively

#### Scenario: Same-time samples preserve recorded order
- **WHEN** two samples share the same simulation timestamp and were recorded in order A then B
- **THEN** the replay emits A before B at that simulation time

### Requirement: Replay exposes a reusable logical source

The Replay logical source SHALL be usable like any other transport source, exposing the recorded source names and advancing the transport sequence for every emitted sample.

#### Scenario: Replay source registers the recorded topics
- **WHEN** a replay is configured from a recording that contains topics `can0` and `video0`
- **THEN** the replay source registers `can0` and `video0` as its output topics before emission begins

#### Scenario: Replay advances transport sequence independently of original sequence
- **WHEN** the replay emits a sample on a topic
- **THEN** the transport sequence of the emitted sample follows the replay source sequence, while the original recorded sequence is preserved in a dedicated metadata field

### Requirement: Replay supports pause, resume, and stop controls

The Replay service SHALL respond to runtime control commands to pause, resume, and stop playback without corrupting the MCAP file or the simulation.

#### Scenario: Pause halts emission without skipping samples
- **WHEN** the replay is paused at simulation time 1.000 s and resumed at 2.000 s
- **THEN** no samples with simulation times between 1.000 s and 2.000 s are emitted before resumption, and playback continues from the same position

#### Scenario: Stop closes the replay source
- **WHEN** the stop command is issued
- **THEN** the replay source stops emitting samples and reports the final simulation time reached

### Requirement: Replay supports configurable loop and speed

The Replay service SHALL support looping the recording and scaling simulation-time progression by a configurable playback speed factor.

#### Scenario: Loop restarts from the first sample
- **WHEN** the replay reaches the last sample and looping is enabled
- **THEN** it emits the first sample again after the original run duration has elapsed in simulation time

#### Scenario: Half-speed stretches timing
- **WHEN** playback speed is set to 0.5
- **THEN** a sample recorded at simulation time 1.000 s is emitted when Chronos reaches 2.000 s of replay time
