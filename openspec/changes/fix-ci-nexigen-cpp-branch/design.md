# Design: Clone the correct nexigen-cpp branch in CI

## Root cause

The `git clone` command in the CI workflow did not specify a branch:

```bash
git clone --depth 1 https://github.com/nexigen-tech/nexigen-cpp.git ../nexigen-ecosystem/nexigen-cpp
```

This clones the default branch (`main`). Local development uses `learning-enhancement`, so CI and local builds diverge.

## Fix

Add `--branch learning-enhancement` to the clone command:

```bash
git clone --depth 1 --branch learning-enhancement https://github.com/nexigen-tech/nexigen-cpp.git ../nexigen-ecosystem/nexigen-cpp
```

This aligns CI with the local development environment.

## Note

This is a short-term alignment. A better long-term solution is to pin `nexigen-cpp` to a specific commit or tag, or to vendor it as a submodule/FetchContent dependency.
