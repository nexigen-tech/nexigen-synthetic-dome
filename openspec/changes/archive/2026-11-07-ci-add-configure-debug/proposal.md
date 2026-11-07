# Proposal: Fix CI configure failures and add debug output

## Problem

CI configure step fails on GitHub Actions but succeeds locally. Two suspected causes:

1. The CMake dependency cache `restore-keys` matches caches from earlier broken configure runs, restoring a potentially corrupted `_deps` directory.
2. The exact configure error is not visible in the public workflow UI because CMake logs are only uploaded as artifacts.

## Goal

- Invalidate the CMake dependency cache by bumping the key prefix so old broken caches are no longer restored.
- Print CMake configure logs directly to the job output when configuration fails, so the error can be diagnosed without downloading artifacts.

## Scope

In scope:
- Bump the CMake dependency cache key prefix from `deps-` to `deps-v2-` and update `restore-keys` accordingly.
- Wrap the `cmake` configure command in `.github/workflows/ci.yml` to print `CMakeOutput.log` and `CMakeError.log` on failure.

Out of scope:
- Fixing any underlying configure error not caused by stale cache (will be done in a follow-up if logs reveal one).

## Success criteria

- CI no longer restores caches from broken configure runs.
- If configuration still fails, logs are visible inline.
