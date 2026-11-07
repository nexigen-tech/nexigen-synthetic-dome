# Design

## Context

The composer and recording test directories use plain executables that return non-zero exit codes on failure. There is no third-party test framework in these directories. The recent chronos test refactor showed that extracting logical scenarios into `static bool test_*()` helpers (or `static int test_*()` helpers that return 0 on success) and simplifying `main()` to a dispatcher works well within these constraints.

## Goals / Non-Goals

**Goals:**
- Reduce cyclomatic complexity of the targeted composer and recording test functions below the >15 threshold where practical.
- Split scenarios by logical boundary: descriptor validation cases, manifest parsing cases, plan emission cases, MCAP writer cases, recorder/replay cases.
- Keep every assertion and every expected value unchanged.
- Ensure all modified tests build and pass.

**Non-Goals:**
- Introducing a test framework.
- Changing runtime behavior or public API.
- Refactoring production parser code in `runtime/composer/src/*`.
- Refactoring files outside the five listed test files.

## Decisions

- **One helper per logical scenario.** For `descriptor_validation_test.cpp` this means one helper per validation category (valid manifest, missing port binding, missing parameter, parameter range, missing thread role, unsupported mode, unknown wire target, incompatible contracts, hash mismatch, missing hash, hardware RT mismatch, undeclared role, duplicate role, multi-instance role conflict). For `manifest_test.cpp` this means helpers for successful parse and each malformed input. For `plan_emission_test.cpp` this means helpers for plan fields and round-trip JSON. For `mcap_writer_helpers_test.cpp` this means internally splitting the largest helpers into setup/read/verify phases. For `recorder_replay_test.cpp` this means one helper per high-level scenario (record-and-replay, blackboard channel, seek-and-speed).
- **Preserve existing return-code style.** Composer tests use `bool` helpers returning `false` on failure and `main()` returning distinct small integers. Recording tests currently use `int` helpers returning 0 on success; keep that convention to minimize churn.
- **Keep helpers `static` where linkage scope allows.** Match existing conventions.
- **No behavioral changes.** Copy assertions verbatim; only control flow structure changes.

## Risks / Trade-offs

- **[Risk]** Loss of per-assertion exit codes in composer tests.  
  **Mitigation:** Distinct codes per helper still make CI failures locatable.
- **[Risk]** Recording tests use `int` helpers with their own internal error codes. Splitting them may change those codes.  
  **Mitigation:** Preserve the original helper's error codes within its sub-helpers, or accept that dispatcher-level codes are sufficient because the helper name identifies the failing scenario.
- **[Risk]** `descriptor_validation_test.cpp` has many builder helpers and a large `main()`.  
  **Mitigation:** Keep the descriptor builders unchanged; only decompose `main()`.
