# Proposal: Add GitHub Actions CI workflow

## Problem

The project now builds and passes tests in seven configurations:

- `build` (default GCC)
- `build-debug` (Debug GCC)
- `build-release` (Release GCC)
- `build-clang` (Clang)
- `build-asan` (AddressSanitizer)
- `build-tsan` (ThreadSanitizer)
- `build-ubsan` (UndefinedBehaviorSanitizer)

However, nothing prevents a future commit from breaking any of these variants. The recent `stabilize-walking-skeleton` change fixed release fixtures, Clang warnings, stale descriptors, and sanitizer incompatibilities manually. Without CI, regressions will reappear silently.

## Goal

Add a GitHub Actions workflow that automatically builds and tests every supported configuration on push and pull request, with caching and clear failure reporting.

## Scope

In scope:
- Create `.github/workflows/ci.yml` with a build/test matrix covering all seven configurations.
- Install required system dependencies (CMake, Ninja, GCC, Clang, Xvfb, OpenGL).
- Install stable Rust toolchain for iceoryx2 and Rust plugins.
- Cache Cargo registry/git/target and CMake `_deps` to keep CI fast.
- Run GUI video tests under Xvfb for default/debug/release configurations.
- Upload test logs as artifacts on failure.
- Add a CI status badge to `README.md`.
- Document branch protection expectations in `AGENTS.md`.

Out of scope:
- Adding new product features or specs.
- Nightly/scheduled builds (can be added later).
- Self-hosted runners or RT hardware testing.

## Success criteria

- A push or PR to `main` or `feature/*` triggers the workflow.
- All seven configurations build and pass tests.
- Failed tests produce downloadable logs.
- `README.md` displays the current CI status.
- The workflow runs in under 15 minutes for a warm cache.
