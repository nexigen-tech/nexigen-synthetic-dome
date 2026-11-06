# Proposal: Add Recording, Replay and Validation

## Motivation

The modelling complex needs deterministic visibility into what happens during a simulation run. Operators and CI pipelines must be able to:

1. Record every typed sample that crosses the transport so that runs can be audited.
2. Replay a recorded run as a deterministic logical source so that components see the same sequence of samples in the same simulation time.
3. Validate replayed or live runs against expectations expressed as predicates over samples and blackboard state.

Without these capabilities, regression testing is manual, HIL/SIL convergence is hard to prove, and debugging non-deterministic behaviour is expensive.

## Scope

### In scope

- A runtime **Recorder** service that subscribes to transport topics and persists samples in MCAP format, preserving simulation time, transport sequence, source name, and typed payload.
- A runtime **Replay** service (and reusable Replay logical source) that reads an MCAP recording and emits samples on the transport in simulation-time order, supporting pause, seek, loop, and speed control through Chronos.
- A runtime **Validation** service that evaluates predicates over live or replayed samples and blackboard updates, producing pass/fail events and a summary report.
- Stable contracts for recorded metadata, replay control, and validation results.
- Integration with existing runtime specs: `runtime/transport`, `runtime/chronos`, `runtime/blackboard`, and `components/testing`.

### Out of scope

- A graphical panel for inspecting recordings (covered by future panel work).
- Compression, encryption, or remote object storage of MCAP files.
- Validation of continuous signals requiring numerical approximation beyond sample-by-sample predicates.

## Success Criteria

1. Any transport topic can be recorded without source code changes and later replayed bit-identically for its payload and sequence metadata.
2. A recorded scenario replayed through the Replay source reproduces the same observable behaviour in deterministic components.
3. Validation rules can be declared in test scenarios and executed during live or replayed runs, producing machine-readable reports.
4. Existing headless tests can assert on recording and validation results.

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| MCAP schema handling for every contract becomes a maintenance burden | Generate MCAP schema records from the canonical schema contract and enforce schema registration at recording start. |
| Replay determinism drifts from live execution due to timing | Bind replay emission strictly to Chronos simulation time, not wall-clock time. |
| High-throughput sources (video) saturate recording I/O | Make recording optional per topic and support writer-side buffering; defer compression to future work. |
| Validation predicates become a mini language | Keep predicates declarative and data-driven, limited to comparisons, presence, and simple aggregation over windows. |

## Dependencies

- `runtime/transport` for typed topic subscription and publication.
- `runtime/chronos` for simulation-time scheduling and replay control.
- `runtime/blackboard` for shared state observation by validation rules.
- `contracts/sample-header` and `contracts/signal-value` for baseline sample metadata.
- `components/testing` for headless assertion integration.
