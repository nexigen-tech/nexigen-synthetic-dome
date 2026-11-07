# Proposal

## Why

Static analysis identified several functions in the host infrastructure and recording runtime with cyclomatic complexity above the project's informal threshold of 15. High complexity in descriptor parsing, plugin loading, component validation, and replay control makes the code harder to test, review, and maintain. Reducing these functions below the threshold improves readability and reduces the risk of regressions without changing external behavior.

## What Changes

- Refactor `nxd::host::load_component_descriptor` in `host/src/descriptor.cpp` by extracting per-section parsers (metadata, ports, parameters, thread roles, supported modes, hardware RT requirements).
- Refactor `nxd::host::parse_ports` in `host/src/descriptor.cpp` into a per-port parsing helper with early-return validation.
- Refactor `nxd::host::parse_parameters` in `host/src/descriptor.cpp` into a per-parameter parsing helper.
- Refactor `nxd::host::load_plugin` in `host/src/plugin.cpp` into discrete stages: ABI/version checks, descriptor hash verification, and vtable initialization.
- Refactor `nxd::host::ComponentInstance::validate_port_bindings` in `host/src/component.cpp` into smaller validators per binding type or error category.
- Refactor `nxd::runtime::recording::ReplaySource::control` in `runtime/recording/src/replay.cpp` by extracting command handlers.
- Refactor `nxd::runtime::recording::ReplaySource::step` in `runtime/recording/src/replay.cpp` by separating frame reading, publishing, and EOF handling.
- Add or extend unit tests where needed to cover the extracted helpers and preserve existing behavior.
- Verify that all target functions drop below cyclomatic complexity 15 and that the full build, test suite, `clang-format`, and `clang-tidy` remain clean.

## Capabilities

### New Capabilities

None. This is a pure internal refactor with no spec-level behavior changes.

### Modified Capabilities

None.

## Impact

- Internal implementation only: no public C++ API, component ABI, contract, or runtime behavior changes.
- Affected files: `host/src/descriptor.cpp`, `host/src/plugin.cpp`, `host/src/component.cpp`, `runtime/recording/src/replay.cpp`, and their tests.
