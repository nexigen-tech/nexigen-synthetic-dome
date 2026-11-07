# Proposal: Clone the correct nexigen-cpp branch in CI

## Problem

The CI workflow clones `nexigen-cpp` without specifying a branch, so it gets the repository default branch (`main`). Local development uses the `learning-enhancement` branch, which is compatible with the current `nexigen-synthetic-dome` code. The default branch is not compatible, causing build or test failures shortly after jobs start.

## Goal

Make the CI clone step check out the `learning-enhancement` branch of `nexigen-cpp`.

## Scope

In scope:
- Update `.github/workflows/ci.yml` to add `--branch learning-enhancement` to the `git clone` command.

Out of scope:
- Pinning to a specific commit or tag.
- Converting the dependency to a submodule.

## Success criteria

- CI uses the same `nexigen-cpp` branch as local development.
- Builds reach the test step and do not fail due to API/behavior mismatches with `nexigen-cpp`.
