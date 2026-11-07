# Proposal

## Why

Static analysis flagged several functions in `runtime/composer` with cyclomatic complexity well above the recommended threshold (`DomePlan::from_json` 39, `SystemManifest::from_json` 29, `validation_rule_from_json` 31, `validate_instance` 25). These large functions mix schema validation, nested object parsing, default-value handling, and error reporting, making the composer harder to test, review, and maintain. We need to refactor them into smaller, composable helpers without changing behavior.

## What Changes

- Refactor `runtime/composer/src/plan.cpp`:
  - Extract `DomePlan::from_json` into per-section helpers (`partitions_from_json`, `let_boundaries_from_json`, `wires_from_json`, `processes_from_json`, `thread_role_assignments_from_json`, `parameter_assignments_from_json`, plus option parsing for `recording`, `replay`, and `validation`).
  - Replace the deeply branched `validation_rule_from_json` with small field-extraction helpers and a table-driven optional-field loop.
- Refactor `runtime/composer/src/manifest.cpp`:
  - Extract `SystemManifest::from_json` into per-section helpers (`components_from_json`, `instances_from_json`, `partitions_from_json`, `wires_from_json`, `let_boundaries_from_json`, `processes_from_json`, `chronos_from_json`).
- Refactor `runtime/composer/src/descriptor_validation.cpp`:
  - Split `validate_instance` into focused validators: required ports, parameters, thread roles, supported modes, hardware-RT constraints, and assigned-role checks.
  - Introduce small predicate helpers and compose validators in a loop.
- Add unit tests for the new helpers to ensure no regression in error paths.
- Run the existing test suite, lint, and static analysis to confirm behavior is unchanged.

## Capabilities

### New Capabilities

None. This is a pure internal refactoring.

### Modified Capabilities

None. No spec-level behavior changes.

## Impact

- Affected files:
  - `runtime/composer/src/plan.cpp`
  - `runtime/composer/src/manifest.cpp`
  - `runtime/composer/src/descriptor_validation.cpp`
  - Related headers and tests under `runtime/composer/`
- No public API or manifest/plan JSON schema changes.
- Existing serialized plans and manifests remain valid.
