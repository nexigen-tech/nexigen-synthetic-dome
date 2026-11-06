# Design: Add GitHub Actions CI workflow

## Runner and triggers

Use `ubuntu-24.04` runners. Trigger on:
- `push` to `main` and `feature/*`
- `pull_request` to `main`

Use `fail-fast: false` so every configuration runs to completion even if one fails.

## Build matrix

| Job name | CMake build dir | CMake flags | Display needed |
|---|---|---|---|
| `default` | `build` | none | yes |
| `debug` | `build-debug` | `-DCMAKE_BUILD_TYPE=Debug` | yes |
| `release` | `build-release` | `-DCMAKE_BUILD_TYPE=Release` | yes |
| `clang` | `build-clang` | `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` | yes |
| `asan` | `build-asan` | `-fsanitize=address` on compile/link flags | no |
| `tsan` | `build-tsan` | `-fsanitize=thread` on compile/link flags | no |
| `ubsan` | `build-ubsan` | `-fsanitize=undefined` on compile/link flags | no |

Sanitizer builds already disable the long video tests (`nxd_host_video_end_to_end` and `nxd_video_viewer_latency`) via CMake. Non-sanitizer builds run GUI tests, so they need `xvfb-run`.

## Dependencies

System packages installed via `apt`:
- `cmake`
- `ninja-build`
- `g++`
- `clang`
- `xvfb`
- `libgl1-mesa-dev`

Rust installed via `dtolnay/rust-toolchain@stable`. Stable is sufficient because Rust plugins are built in release mode and sanitizer instrumentation is only required on the C++ side (the CMake sanitizer logic already disables the long video tests, and the Rust plugins load correctly once descriptors match).

## Caching

Two cache steps per job:

1. **Cargo cache:**
   - Paths: `~/.cargo/registry`, `~/.cargo/git`, `target`
   - Key: `${{ runner.os }}-cargo-${{ hashFiles('**/Cargo.lock') }}`
   - Restore key: `${{ runner.os }}-cargo-`

2. **CMake dependency cache:**
   - Path: `${{ matrix.build-dir }}/_deps`
   - Key: `${{ runner.os }}-${{ matrix.name }}-deps-${{ hashFiles('CMakeLists.txt') }}`
   - This avoids re-downloading iceoryx2 on every run.

## Build and test steps

1. Configure: `cmake -S . -B <build-dir> <flags>`
2. Build: `cmake --build <build-dir> -j$(nproc)`
3. Test:
   - For default/debug/release/clang: `xvfb-run ctest --test-dir <build-dir> --output-on-failure -j$(nproc)`
   - For sanitizer builds: `ctest --test-dir <build-dir> --output-on-failure -j$(nproc)`

## Failure artifacts

On test failure, upload `Testing/Temporary/LastTest.log` and any sanitizer output as artifacts so developers can diagnose without re-running CI locally.

## README badge

Add a status badge near the top of `README.md`, replacing the placeholder `![Build Status](https://shields.io)` link.

## AGENTS.md note

Create or update `AGENTS.md` with a CI section documenting:
- The seven supported build variants.
- How to run them locally.
- That PRs must pass CI before merge.
- The branch protection rule expectation.
