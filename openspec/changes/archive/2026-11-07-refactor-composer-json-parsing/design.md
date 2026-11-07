# Design

## Context

The composer is responsible for turning JSON system manifests and execution plans into strongly-typed C++ structures and for validating those structures against component descriptors. Static analysis showed that the top-level `from_json` and `validate_instance` functions have become monolithic, with cyclomatic complexities in the high twenties and thirties. The code is correct, but the structure makes unit testing individual validation rules difficult and increases the risk of merge conflicts and bugs during future schema evolution.

The project compiles with `-fno-exceptions -fno-rtti` and uses `nlohmann::json` in no-exception mode (`JSON_NOEXCEPTION`). All parsing helpers therefore return `std::optional<T>` or `bool` and must remain `noexcept`. This design preserves those constraints.

## Goals / Non-Goals

**Goals:**
- Reduce cyclomatic complexity of `DomePlan::from_json`, `SystemManifest::from_json`, `validation_rule_from_json`, and `validate_instance` below 15.
- Keep every helper small, single-purpose, and unit-testable.
- Preserve exact JSON schema compatibility and error behavior (any invalid input that currently returns `std::nullopt` or emits a diagnostic must continue to do so).
- Maintain the existing `noexcept` contract and zero-allocation cold-path semantics where already present.

**Non-Goals:**
- No changes to public headers or data structures.
- No changes to the JSON schema or serialization format.
- No introduction of exceptions, RTTI, or new external dependencies.
- No behavioral changes to validation rules (e.g., tightening or relaxing checks).

## Decisions

### 1. One helper per JSON section

`DomePlan::from_json` and `SystemManifest::from_json` will delegate each top-level array/object to a dedicated helper:

```text
DomePlan::from_json
  ├─ parse_required_uint32 / parse_required_object helpers
  ├─ chronos_config_from_json (already exists)
  ├─ partitions_from_json
  ├─ let_boundaries_from_json
  ├─ wires_from_json
  ├─ processes_from_json
  ├─ thread_role_assignments_from_json
  ├─ parameter_assignments_from_json
  ├─ recording_options_from_json (already exists)
  ├─ replay_options_from_json (already exists)
  └─ validation_options_from_json (already exists)

SystemManifest::from_json
  ├─ component_refs_from_json
  ├─ instances_from_json
  ├─ partitions_from_json
  ├─ wires_from_json
  ├─ let_boundaries_from_json
  ├─ processes_from_json
  └─ chronos_config_from_json (already exists)
```

Each helper checks that the field exists, is the expected type, parses every element, and appends to the output container. They return `bool` so the caller can early-return on failure. This removes the repetitive `find`/`is_array`/`push_back` triads from the top-level function.

### 2. Generic array parser helper

Both files duplicate the pattern of iterating a JSON array and calling an element parser. A single templated helper will be added in each anonymous namespace:

```cpp
template <typename T, typename ParseFn>
[[nodiscard]] bool parse_array(const nlohmann::json& j,
                               std::vector<T>& out,
                               ParseFn&& parse) noexcept;
```

This helper checks `is_array()`, iterates elements, calls `parse`, and returns `false` on any failure. It is intentionally simple and does not allocate on failure.

### 3. Table-driven optional fields in `validation_rule_from_json`

`validation_rule_from_json` currently has a separate branch for every optional field. We will keep required fields (`name`, `op`, `field`) as explicit early checks, then process optional fields with a small set of typed extractors:

- `assign_if_string`
- `assign_if_number`
- `assign_if_boolean`
- `assign_if_object<T, parser>`
- `assign_if_hash_string`

Each extractor returns `bool` and is responsible for type checking and assignment. `validation_rule_from_json` will call them in a flat sequence, short-circuiting on the first failure. This reduces the function to roughly one branch per extractor call instead of one branch per type check plus one branch per assignment.

### 4. Composed validators for `validate_instance`

`validate_instance` will be split into standalone validator functions, each taking `(const SystemManifest&, const Instance&, const ComponentDescriptor&, std::vector<ValidationDiagnostic>&)`:

- `validate_required_ports`
- `validate_parameters`
- `validate_required_thread_roles`
- `validate_supported_mode`
- `validate_hardware_rt_requirement`
- `validate_assigned_thread_roles`

`validate_instance` will call them in order. Each validator remains `noexcept` and appends diagnostics. This does not change the emitted diagnostics or their order, but it makes each rule independently testable.

### 5. Keep helpers in anonymous namespaces

All new helpers will live in the existing anonymous namespaces inside `plan.cpp`, `manifest.cpp`, and `descriptor_validation.cpp`. No new public symbols are introduced, so ABI and compile times are unaffected.

## Risks / Trade-offs

- **[Risk]** Mechanical refactor could accidentally reorder diagnostics or change short-circuit behavior.
  - **Mitigation:** Add focused unit tests for the refactored helpers before/after the change and run the full composer test suite.
- **[Risk]** Generic `parse_array` template could bloat compile times if instantiated many times in the same translation unit.
  - **Mitigation:** It will be a single small template used for a handful of element types; inlining is desirable here. If compile-time impact is measurable, we can provide non-template overloads later.
- **[Risk]** `validation_rule_from_json` refactor could make stepping through with a debugger slightly less direct.
  - **Mitigation:** Each extractor is named after the field it handles, preserving readability.

## Migration Plan

Not applicable. This is a source-only refactor with no runtime migration or deployment step.

## Open Questions

None.
