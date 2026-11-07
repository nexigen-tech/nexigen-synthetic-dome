# Proposal: Fix CI workflow — clone missing nexigen-cpp dependency

## Problem

The CI workflow added in `add-ci-workflow` triggered successfully on the first push, but all seven build jobs failed. The project depends on a sibling repository `../nexigen-ecosystem/nexigen-cpp`, which is not present in the GitHub Actions checkout. CMake configuration or the build step fails because `NEXIGEN_CPP_ROOT` points to a non-existent directory.

## Goal

Make the CI workflow self-contained by cloning the `nexigen-cpp` sibling dependency before configuring CMake, and improve failure artifact collection.

## Scope

In scope:
- Add a workflow step to shallow-clone `https://github.com/nexigen-tech/nexigen-cpp.git` into `../nexigen-ecosystem/nexigen-cpp`.
- Broaden the failure artifact upload to include CMake configure logs and ignore missing files gracefully.

Out of scope:
- Changing the local development workflow or converting the dependency to a submodule/FetchContent.

## Success criteria

- A push to `feature/*` triggers CI and all seven configurations reach the test step.
- Test failures produce downloadable logs when available.
