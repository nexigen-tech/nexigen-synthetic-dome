# Agent Guide: NexDome

This document helps AI agents and human contributors navigate the `nexigen-synthetic-dome` repository.

## Project overview

NexDome is a high-fidelity Synthetic Environment and Hardware-in-the-Loop (HIL) platform. It is built in Modern C++23 with Rust components, uses Eclipse iceoryx2 for zero-copy IPC, and is organized as a walking skeleton with runtime modules, components, panels, and tools.

## Repository layout

| Directory | Purpose |
|-----------|---------|
| `cpp/` | Integration with the sibling `nexigen-cpp` library. |
| `host/` | `nxd-host` runtime: plugin loading, headless harness, descriptor handling. |
| `runtime/` | Core modules: `chronos`, `composer`, `transport`, `blackboard`, `recording`, `validation`. |
| `components/` | Example and test components: `virtual_can`, `virtual_video`, `can_monitor`, `scenario_control`, `scenario_consumer`, `video_opengl`. |
| `sdk/` | C++ and Rust SDKs for writing components (`sdk/cpp`, `sdk/rust`). |
| `contracts/` | YAML contract definitions consumed by `nxdc` to generate C++ headers and component descriptors. |
| `tools/` | `nexigen-run` CLI, `nxdc` descriptor compiler, `video-viewer`. |
| `panels/` | Tauri + Svelte panel skeleton (`scenario-panel`). |
| `tests/` | Host tests and component test plugins in C++ and Rust. |
| `examples/` | Example C++ and Rust components. |
| `openspec/` | OpenSpec planning artifacts and durable specs. |

## Supported build variants

The project supports seven CMake build configurations. Use separate build directories so artifacts do not collide.

| Directory | Type | Command |
|-----------|------|---------|
| `build` | default GCC | `cmake -S . -B build` |
| `build-debug` | Debug GCC | `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug` |
| `build-release` | Release GCC | `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` |
| `build-clang` | Clang | `cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` |
| `build-asan` | AddressSanitizer | `cmake -S . -B build-asan -DCMAKE_CXX_FLAGS=-fsanitize=address -DCMAKE_C_FLAGS=-fsanitize=address -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address` |
| `build-tsan` | ThreadSanitizer | same pattern with `-fsanitize=thread` |
| `build-ubsan` | UBSan | same pattern with `-fsanitize=undefined` |

Build and test:

```bash
cmake --build <build-dir> -j$(nproc)
ctest --test-dir <build-dir> --output-on-failure -j$(nproc)
```

GUI video tests (`nxd_video_viewer_latency`) require a display. On a headless machine use `xvfb-run`:

```bash
xvfb-run ctest --test-dir build --output-on-failure -j$(nproc)
```

Sanitizer builds automatically disable the long video tests (`nxd_host_video_end_to_end` and `nxd_video_viewer_latency`) because they are not meaningful under heavy instrumentation.

## Code formatting and static analysis

Configure any build directory (the default `build` is recommended) and use the dedicated targets:

```bash
cmake -S . -B build
cmake --build build --target nxd_clang_format -j$(nproc)
cmake --build build --target nxd_clang_tidy -j$(nproc)
```

- `nxd_clang_format` runs `clang-format --dry-run --Werror` over all project C++ sources and headers.
- `nxd_clang_format_apply` rewrites sources in place to match `.clang-format`.
- `nxd_clang_tidy` runs `clang-tidy` over all project C++ translation units using the selected build directory's `compile_commands.json`.

These checks are enforced in CI via the `lint` job in `.github/workflows/ci.yml`.

## Dependencies

- CMake >= 3.25
- C++23 compiler (GCC or Clang)
- Stable Rust toolchain (for `cargo` builds and `nxdc`)
- clang-format and clang-tidy (for lint targets)
- OpenGL development libraries
- Xvfb (for headless GUI tests)

Ubuntu example:

```bash
sudo apt-get install cmake ninja-build g++ clang clang-format clang-tidy xvfb libgl1-mesa-dev
```

## Component descriptors

Component descriptors and their hashes are generated from `components/*.yaml` and `contracts/*.yaml` into `<build-dir>/generated/descriptors/`. CMake tracks contract files as dependencies, so editing a contract triggers regeneration. If you ever see `load failed` in an older build directory, reconfigure and delete stale descriptors:

```bash
cmake -S . -B <build-dir>
rm -f <build-dir>/generated/descriptors/*.{json,hash}
cmake --build <build-dir> --target nxd_component_descriptors
```

## Rust plugins

Rust plugins are built with `cargo build --release` into `target/release/`. They are shared across all CMake build directories. The release profile avoids the unwind runtime injected by Cargo's dev profile. Sanitizer builds work with these release Rust plugins as long as component descriptors match.

## CI

GitHub Actions runs the workflow in `.github/workflows/ci.yml` on every push to `main` or `feature/*` and on every PR to `main`. It builds and tests all seven variants with caching for Cargo and CMake dependencies.

### Branch protection

The `main` branch should require the CI workflow to pass before merging. Configure this in GitHub repository settings under **Branches > Add rule**:

- Require a pull request before merging.
- Require status checks to pass before merging.
- Search for and enable the `lint` job check and the `build-and-test` job checks (e.g., `default`, `release`, `clang`, `asan`, `tsan`, `ubsan`).

## Conventions

- C++ code uses `-fno-exceptions -fno-rtti -Wall -Wextra -Werror`.
- Cold path may allocate; after `arm()` components must not allocate, block, or throw.
- All samples, commands, and external events carry a deterministic `frame_no`/`sim_time_ns` identity.
- Use OpenSpec for non-trivial changes: `openspec new change <name>`, implement, archive with `openspec archive`.
