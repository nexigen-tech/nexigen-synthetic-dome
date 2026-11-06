# Tasks

## 1. Recording contracts and schema registry

- [x] 1.1 Define canonical schemas for `RecordingMetadata`, `ReplayControl`, and `ValidationResult` and verify they pass schema validation (`openspec validate --schemas` or the project equivalent).
- [x] 1.2 Register the new schemas in the schema registry with stable identifiers and verify generated headers/bindings compile.

## 2. Recorder service

- [x] 2.1 Implement Recorder transport subscription by source name and verify it receives samples in publication order.
- [x] 2.2 Implement MCAP writer with per-topic channels and schema records, and verify a recorded file is readable by `mcap info`.
- [x] 2.3 Persist simulation time, transport sequence, source name, and schema identifier with each sample, and verify round-trip equality.
- [x] 2.4 Add recording of blackboard updates on the reserved `_blackboard` pseudo-topic and verify updates appear in the MCAP file.
- [x] 2.5 Close the MCAP file gracefully on run end and write the `RecordingMetadata` attachment; verify the file is valid after stop.

## 3. Replay service and logical source

- [x] 3.1 Implement MCAP reader that orders messages by simulation time and verify iteration order matches recorded timing.
- [x] 3.2 Implement Replay logical source that registers topics from the recording and verify downstream components can subscribe to replayed topics.
- [x] 3.3 Bind replay emission to Chronos simulation time and verify samples are emitted at original simulation times.
- [x] 3.4 Implement pause, resume, stop, seek, loop, and speed control via `ReplayControl` and verify each command through headless tests.
- [x] 3.5 Preserve original transport sequence in metadata while advancing replay source sequence, and verify both values are present on emitted samples.

## 4. Validation service

- [x] 4.1 Implement Validation service subscription to declared topics and blackboard keys and verify it receives the expected events.
- [x] 4.2 Implement declarative predicate evaluation for threshold, presence, field-to-constant, and field-to-field comparisons and verify each predicate type through unit tests.
- [x] 4.3 Emit pass/fail validation events per sample and verify events carry rule name, simulation time, and reason.
- [x] 4.4 Produce the `ValidationResult` report at run end and verify it includes outcome, counts, and failure details.
- [x] 4.5 Run validation during live recording and replay of the same recording, and verify both reports are identical.

## 5. Integration and examples

- [x] 5.1 Add configuration options to the runner for enabling Recorder, Replay, and Validation services and verify the runner loads them without errors.
- [x] 5.2 Create a headless test scenario that records a CAN signal sequence, replays it, and validates a threshold rule; verify the test passes.
- [x] 5.3 Document the new services, configuration keys, and rule format in the project docs and verify the documented commands run as written.

## 6. Workflow follow-up

- Archive the change after the project's review requirements are satisfied.
- Verify the archived result with `openspec status --change add-recording-replay-and-validation`.
