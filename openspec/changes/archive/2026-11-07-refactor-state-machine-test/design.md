# Design

## Context

`runtime/chronos/tests/state_machine_test.cpp` currently uses a single `main()` function containing all state-machine lifecycle assertions. The project does not use a third-party test framework in this directory; tests are plain executables that return non-zero exit codes on failure. The refactor must keep this convention while improving readability and reducing cyclomatic complexity.

## Goals / Non-Goals

**Goals:**
- Split the monolithic `main()` into named helper functions, one per logical test scenario.
- Keep each helper self-contained and deterministic.
- Preserve all existing assertions and failure semantics.
- Ensure the test continues to compile with `-fno-exceptions -fno-rtti -Wall -Wextra -Werror` and passes under `ctest`.

**Non-Goals:**
- Introducing a test framework (Catch2, GTest, doctest).
- Changing any runtime behavior or state-machine semantics.
- Modifying other chronos tests.

## Decisions

- **One helper per logical scenario.** Each block from the original `main()` becomes a `static bool test_*()` function. This maps directly to the existing comment boundaries and makes failures trivial to locate.
- **`bool` return instead of per-assertion exit codes.** Helpers return `false` on any failed assertion; `main()` maps each helper to a small integer exit code. This avoids magic return-code arithmetic inside the helpers while retaining the project's plain-executable test style.
- **Keep helper functions `static`.** Limits linkage scope and matches the existing `static` helpers (`ok`, `failed`, `step_config`, `scaled_config`).
- **No behavioral changes.** Assertions, configuration values, and expected state transitions are copied verbatim.

## Risks / Trade-offs

- **[Risk]** Loss of fine-grained exit codes (previously each assertion had a unique code).  
  **Mitigation:** `main()` still returns distinct codes per helper, preserving enough granularity for CI logs. The helper name appears in the source and is easy to map to the exit code.
- **[Risk]** Future regressions in helpers are harder to bisect without a framework.  
  **Mitigation:** Scope is intentionally narrow; if bisection becomes important, the project can adopt a framework across all chronos tests later.
