# Recording, Replay, and Validation

This document describes the optional runtime services that record transport
samples to MCAP files, replay previously recorded samples as a logical source,
and evaluate declarative validation rules over live or replayed data.

These services are completely optional.  When they are not configured in a Dome
Plan, `nexigen-run` behaves exactly as before and existing components see no
change in the transport wire format or port API.

## Overview

| Service | Purpose | Lives in |
|---------|---------|----------|
| Recorder | Subscribes to configured transport topics and persists every sample, plus blackboard updates, to one MCAP file per run. | `runtime/recording` |
| Replay | Reads an MCAP file and re-emits samples on the transport in original simulation-time order. | `runtime/recording` |
| Validation | Subscribes to topics and blackboard keys named by rules and evaluates per-sample predicates. | `runtime/validation` |

The services share a single in-process `HeadlessTransport` instance owned by
`nexigen-run`.  They are created, stepped, and torn down around the single
Chronos frame driven by the walking-skeleton runner.

## Configuration

All configuration lives in the Dome Plan consumed by `nexigen-run`.  The three
optional top-level sections are `recording`, `replay`, and `validation`.

### Recorder

```json
{
  "recording": {
    "enabled": true,
    "output_path": "recorded.mcap",
    "topics": [
      {
        "source_name": "telemetry",
        "element_size": 4,
        "max_count": 1,
        "layout_hash": "0000000000000000000000000000000000000000000000000000000000000000",
        "schema_name": "SignalValueF32@1.0.0"
      }
    ]
  }
}
```

Keys:

- `enabled` – `true` to start the Recorder, `false` to leave it disabled.
- `output_path` – path to the MCAP file written for this run.
- `topics` – list of transport topics to subscribe to.
  - `source_name` – transport source name.
  - `element_size` – size of one sample element in bytes.
  - `max_count` – maximum number of elements in one sample.
  - `layout_hash` – 64-character hex encoding of the 32-byte contract layout
    hash; used for transport registration and replay metadata.
  - `schema_name` – canonical schema identifier stored in the MCAP channel.

The Recorder also writes a reserved `_blackboard` pseudo-topic whenever
`record_blackboard()` is called, so blackboard state transitions can be audited
and replayed.

### Replay

```json
{
  "replay": {
    "enabled": true,
    "input_path": "replay.mcap",
    "loop": false,
    "speed": 1.0,
    "autostart": true
  }
}
```

Keys:

- `enabled` – `true` to load and run the Replay source.
- `input_path` – path to the MCAP file produced by a previous recording.
- `loop` – if `true`, restart from the first sample after reaching the end.
- `speed` – playback speed multiplier; a value of `0.5` emits a sample at
  original time `t` when Chronos reaches `2t`.
- `autostart` – if `true`, send the `Start` command as soon as the file is
  loaded.

Replay registers the topics found in the MCAP file as transport publishers and
emits samples when `ReplaySource::step(sim_time_ns)` is called by the runner.

### Validation

```json
{
  "validation": {
    "enabled": true,
    "rules": [
      {
        "name": "telemetry_below_100",
        "topic": "telemetry",
        "op": "less_than",
        "field": {
          "path": "value",
          "offset": 0,
          "size": 4,
          "is_floating": true,
          "is_signed": false
        },
        "has_constant": true,
        "constant": 100.0,
        "element_size": 4,
        "max_count": 1,
        "layout_hash": "0000000000000000000000000000000000000000000000000000000000000000"
      }
    ]
  }
}
```

Keys:

- `enabled` – `true` to start the Validation service.
- `rules` – list of rules to evaluate.
  - `name` – human-readable rule identifier, echoed in the final report.
  - `topic` – transport source name for non-blackboard rules.
  - `blackboard_key` – blackboard key for blackboard rules.
  - `op` – predicate operator (see below).
  - `field` – primary field specification.
  - `reference_field` – optional second field for field-to-field comparisons.
  - `constant` / `has_constant` – constant value for field-to-constant rules.
  - `constant_string` – reserved for future string-valued comparisons.
  - `is_blackboard_rule` – `true` if this rule reads a blackboard key.
  - `reason_template` – optional fixed failure reason.
  - `element_size`, `max_count`, `layout_hash` – transport metadata required for
    non-blackboard rules.

#### Predicate operators

| `op` | Meaning |
|------|---------|
| `less_than` | field < reference |
| `less_or_equal` | field <= reference |
| `greater_than` | field > reference |
| `greater_or_equal` | field >= reference |
| `equal` | field == reference (within 1e-6 for floating point) |
| `not_equal` | field != reference |
| `present` | field exists (always evaluates to true for fixed samples) |

The reference is either `constant` when `has_constant` is `true`, or the value
read from `reference_field` when `reference_field.size > 0`.

## Running the services

Build the runner:

```bash
cmake --build build --target nexigen-run
```

Run the walking-skeleton test that enables all three services:

```bash
ctest --test-dir build -R nxd_nexigen_run_services --output-on-failure
```

Or run the fixture plan directly from the fixture directory so the relative
paths in the plan resolve:

```bash
cd build/tools/nexigen-run/fixtures
../nexigen-run ./nxd-ws01-services.json
```

The services test fixture is at
`tools/nexigen-run/tests/fixtures/nxd-ws01-services.json`.  It replays one
`telemetry` sample with value `50.0`, records the replayed output, and validates
that the value is below `100.0`.

## Implementation notes

- The MCAP writer/reader is intentionally minimal: MCAP 0, no chunks, no
  compression.  It is used only for in-process tests and audit files.
- Each run is identified by a 16-byte UUID stored in
  `RecordingMetadata.run_id`.
- Blackboard updates are recorded on the reserved `_blackboard` pseudo-topic.
- Replay preserves the original simulation time and transport sequence in the
  emitted sample metadata while advancing a fresh replay sequence.
- Validation produces a `ValidationResult` report at run end with overall
  outcome, pass/fail counts, and per-failure details.

## See also

- `runtime/recording/include/nxd/runtime/recording/recorder.hpp`
- `runtime/recording/include/nxd/runtime/recording/replay.hpp`
- `runtime/validation/include/nxd/runtime/validation/validation.hpp`
- `contracts/RecordingMetadata.yaml`
- `contracts/ReplayControl.yaml`
- `contracts/ValidationResult.yaml`
