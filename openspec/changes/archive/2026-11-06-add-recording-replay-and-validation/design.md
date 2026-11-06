# Design: Recording, Replay and Validation

## Context

The runtime already provides typed transport topics (`runtime/transport`), simulation-time scheduling (`runtime/chronos`), and shared state (`runtime/blackboard`). Components publish samples on named sources and receive them through typed ports. The project uses MCAP as the canonical recording container format and has stable contracts for samples (`contracts/sample-header`, `contracts/signal-value`, `contracts/video`, `contracts/can`).

See `proposal.md` for motivation and scope. This design describes how the Recorder, Replay source, and Validation service fit into the existing runtime without changing component authoring rules.

## Goals / Non-Goals

**Goals:**
- Add a Recorder service that can subscribe to any transport topic and persist samples into a single MCAP file per run.
- Add a Replay logical source that reads an MCAP file and re-emit samples in simulation-time order.
- Add a Validation service that evaluates declarative predicates over samples and blackboard state.
- Keep recording, replay, and validation optional so that existing components run unchanged when the services are not configured.

**Non-Goals:**
- Changing the transport wire format or port API.
- Adding a GUI panel for browsing recordings.
- Supporting distributed storage or encryption of MCAP files.
- Real-time streaming of recordings to external systems.

## Decisions

### Decision: Recorder is a runtime service, not a source

The Recorder will be implemented as a runtime service that subscribes to transport topics, similar to how other runtime services consume telemetry. This avoids requiring every source to know about recording and keeps the source contract unchanged.

**Rationale:** Sources should not be burdened with persistence concerns. A central service can manage file lifecycle, schema registration, and metadata in one place.

**Alternative considered:** Recording inside each source. Rejected because it duplicates file management and complicates source implementations.

### Decision: Use MCAP as the container format

MCAP is already used informally in the project. The Recorder will write MCAP files with one channel per source topic, plus a reserved channel for blackboard updates. Each MCAP message stores the serialized payload, simulation timestamp, and transport sequence number.

**Rationale:** MCAP supports multiple channels, schema records, attachments, and fast indexed seeking, which satisfies recording, replay, and third-party tooling needs.

**Alternative considered:** A custom binary format. Rejected because it would require writing custom viewers and replay tooling.

### Decision: Replay is implemented as a logical source

The Replay service will expose a logical source that registers the topics present in the recording and emits samples through the transport at the appropriate simulation times. This lets existing components consume replayed data without knowing whether the source is live or recorded.

**Rationale:** Reusing the source abstraction keeps replay compatible with the existing typed-port infrastructure and deterministic scheduling.

**Alternative considered:** A direct component-to-MCAP reader. Rejected because it would bypass transport ordering and make replayed components behave differently from live ones.

### Decision: Validation rules are declared, not coded

Validation rules will be declared in scenario or test configuration as predicates. The Validation service will subscribe to the referenced topics and blackboard keys, evaluate the predicate for each sample or update, and produce events. This lets CI scenarios express expectations without custom test code.

**Rationale:** Declarative rules are easier to review, reuse across scenarios, and generate reports from.

**Alternative considered:** Hard-coded validation components. Rejected because it would create a new component type for every rule and discourage reuse.

### Decision: Blackboard updates are recorded as a reserved pseudo-topic

Blackboard updates will be serialized and recorded on a reserved source name (e.g., `_blackboard`). During replay, these updates can be emitted back to the blackboard so that validation rules and components observe the same state transitions as in the original run.

**Rationale:** Treating blackboard state as a first-class recorded stream simplifies deterministic replay and validation.

**Alternative considered:** Snapshotting blackboard state periodically. Rejected because it loses the timing of state transitions and complicates validation.

### Decision: Recording contracts use the canonical schema format

`RecordingMetadata`, `ReplayControl`, and `ValidationResult` will be defined as canonical schemas. They will be registered in the schema registry and referenced by stable identifiers.

**Rationale:** Using the same schema mechanism as other contracts enables code generation and consistent serialization across languages.

## Risks / Trade-offs

| Risk / Trade-off | Mitigation |
|------------------|------------|
| High-throughput topics such as video may saturate the recorder's writer thread | Recording is optional per topic and can be buffered; long-term compression or sharding is deferred. |
| Replay determinism depends on Chronos scheduling fidelity | Replay binds emission to simulation time and never to wall-clock time. |
| Validation rules may become too expressive and hard to optimize | Keep the rule language restricted to comparisons, presence, field references, and a small set of aggregations over fixed windows. |
| MCAP schema records must stay in sync with canonical schemas | The Recorder registers schema records by looking up the schema identifier from the schema registry at startup. |
| Recording blackboard updates adds traffic on the reserved topic | Blackboard updates are typically low frequency compared to signal samples, so the overhead is acceptable. |

## Migration Plan

1. Introduce the recording contracts (`RecordingMetadata`, `ReplayControl`, `ValidationResult`) into the schema registry.
2. Implement the Recorder service behind a feature flag in the runner configuration.
3. Implement the Replay logical source and expose it in scenario configurations.
4. Implement the Validation service and integrate it with the headless testing framework.
5. Add example scenarios that record, replay, and validate a simple CAN signal sequence.
6. Remove feature flags once the services are covered by tests.

## Open Questions

- What is the exact reserved source name for blackboard updates? (`_blackboard` is proposed; confirm naming convention with the runtime team.)
- Should the Validation service support rules that span multiple samples with explicit time windows, or only per-sample predicates in the first iteration?
