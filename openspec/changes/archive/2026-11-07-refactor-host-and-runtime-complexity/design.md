# Design

## Context

Static analysis flagged seven functions with cyclomatic complexity above 15 in the host infrastructure and recording runtime. The highest are concentrated in `host/src/descriptor.cpp`, where component descriptor parsing mixes JSON validation, type conversions, and structural checks in a single long function. The recording runtime's `ReplaySource::control` and `step` similarly combine command dispatch, state transitions, and I/O logic.

This change is a follow-up to the earlier composer JSON parsing refactor (`refactor-composer-json-parsing`). The same strategy applies: extract small, single-purpose helpers with early-return semantics, keep them in anonymous namespaces, and preserve the existing public interface and behavior.

See `proposal.md` for the list of target functions and motivation.

## Goals / Non-Goals

**Goals:**
- Reduce cyclomatic complexity of all seven target functions below 15.
- Keep the public API and runtime behavior unchanged.
- Preserve the order of validation and diagnostic messages where they are observable by tests.
- Add regression tests for the new helpers where gaps exist.
- Verify the change with build, tests, `clang-format`, `clang-tidy`, and the project analyzer.

**Non-Goals:**
- No new features or spec-level behavior changes.
- No changes to component contracts, ABI, or generated descriptors.
- No restructuring of file layout or namespace boundaries beyond extracting helpers.

## Decisions

1. **Reuse the composer refactor pattern**
   - Extract typed parsing helpers (`parse_string`, `parse_uint32`, `parse_bool`, `parse_array`, etc.) and per-section parsers in anonymous namespaces.
   - Rationale: Proven in the composer refactor, consistent style, easy to test, and keeps implementation details hidden.

2. **Preserve observable validation order**
   - Descriptor parsing validates fields in a fixed order and reports failures at the first invalid field. The refactored helpers will maintain that order.
   - Rationale: Existing tests may assert specific error messages or early-exit behavior; changing the order would be a behavioral change.

3. **Split `load_plugin` by lifecycle stage**
   - Extract `verify_abi_version`, `verify_descriptor_hash`, and `initialize_vtable` helpers.
   - Rationale: Each stage has a clear precondition and failure mode, making the function easier to reason about and unit-test.

4. **Split `validate_port_bindings` by concern**
   - Separate validation of required bindings, duplicate checks, and type compatibility into small validators.
   - Rationale: Reduces nesting and makes each failure path independently testable.

5. **Split replay control by command**
   - Extract handlers for each `ReplayControl` command (`play`, `pause`, `seek`, `stop`, etc.) in `ReplaySource::control`.
   - Rationale: Command dispatch is naturally branch-heavy; per-command handlers flatten the function.

6. **Split replay step into phases**
   - Separate `read_next_message`, `publish_message`, and `handle_eof` in `ReplaySource::step`.
   - Rationale: Each phase has distinct responsibilities and error handling.

## Risks / Trade-offs

- **[Risk] Subtle behavior change in plugin loading** → Mitigation: preserve exact return paths and error strings; add tests for each failure stage.
- **[Risk] Tests depend on internal function structure** → Mitigation: helpers are added, not removed; existing tests continue to compile against public types.
- **[Risk] New helpers increase total line count** → Mitigation: this is acceptable if complexity and readability improve.
