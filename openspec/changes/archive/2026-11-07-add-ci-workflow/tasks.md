# Tasks: Add GitHub Actions CI workflow

## 1. Create CI workflow

- [x] 1.1 Create `.github/workflows/ci.yml` with a build/test matrix for default, debug, release, clang, asan, tsan, and ubsan.
- [x] 1.2 Install system dependencies: cmake, ninja-build, g++, clang, xvfb, libgl1-mesa-dev.
- [x] 1.3 Install stable Rust toolchain.
- [x] 1.4 Add Cargo cache for `~/.cargo/registry`, `~/.cargo/git`, and `target`.
- [x] 1.5 Add CMake `_deps` cache per build variant.
- [x] 1.6 Run non-sanitizer tests under `xvfb-run` so GUI video tests work headless.
- [x] 1.7 Upload `Testing/Temporary/LastTest.log` and sanitizer output as artifacts on failure.

## 2. Update README

- [x] 2.1 Replace the placeholder build-status badge with a real GitHub Actions status badge.

## 3. Create AGENTS.md

- [x] 3.1 Create `AGENTS.md` with project onboarding: structure, build variants, test commands, CI expectations, and branch protection note.

## 4. Verify workflow

- [x] 4.1 Validate the YAML syntax of `.github/workflows/ci.yml`.
- [x] 4.2 Run a local dry-run if possible (`act` or `python -c` yaml parse).
- [x] 4.3 Commit and push; confirm the workflow appears in the GitHub Actions tab.

## 5. Archive change

- [x] 5.1 Mark all tasks complete.
- [x] 5.2 Run `openspec archive` for `add-ci-workflow`.
