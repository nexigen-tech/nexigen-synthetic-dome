# Design

## Context

The chronos test directory uses plain executables that return non-zero exit codes on failure. There is no third-party test framework in this directory, so the refactor must keep that convention. The previous `state_machine_test.cpp` refactor showed that extracting logical scenarios into `static bool test_*()` helpers and simplifying `main()` to a dispatcher works well within these constraints.

## Goals / Non-Goals

**Goals:**
- Reduce cyclomatic complexity of chronos test `main()` functions and helpers below the >15 threshold where practical.
- Split scenarios by logical boundary: lifecycle mode, state-based rejection, partition behavior, C API contract.
- Keep every assertion and every expected value unchanged.
- Ensure all modified tests build and pass.

**Non-Goals:**
- Introducing a test framework.
- Changing runtime behavior or public API.
- Refactoring test files outside `runtime/chronos/tests/` in this change.

## Decisions

- **One helper per logical scenario.** For `state_machine_test.cpp` this means state-based helpers. For `mode_policy_test.cpp` this means one helper per execution mode plus the play/pause lifecycle check. For `frame_distribution_test.cpp` this means separate helpers for partition registration, latest-context consistency, late-registration rejection, HIL hardware time, and concurrent readers. For `c_api_test.cpp` this means helpers grouped by C API contract (lifecycle, partition access, error codes).
- **Shared `base_ns` constant.** Keep existing constants; do not restructure initialization unless it reduces complexity.
- **`bool` returns, dispatcher `main()`.** Helpers return `false` on any failed assertion; `main()` returns a distinct small-integer exit code per helper. This matches the established pattern.
- **Keep helpers `static`.** Same linkage-scope convention as existing code.
- **No behavioral changes.** Copy assertions verbatim; only control flow structure changes.

## Risks / Trade-offs

- **[Risk]** Loss of per-assertion exit codes.  
  **Mitigation:** Distinct codes per helper still make CI failures locatable; helper names map directly to exit codes.
- **[Risk]** Helpers for `frame_distribution_test.cpp` share a `Runtime` and `Partition` state, which complicates splitting.  
  **Mitigation:** Use function parameters to pass `Runtime*`/`Partition*` where state must be shared, or create self-contained helpers with their own `Runtime` instances where possible.
