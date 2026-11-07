# Tasks

## 1. System Manifest + Dome Plan

- [ ] 1.1 Create `plans/composer-can-demo/manifest.json` — System Manifest declaring can_source and can_sink instances in a single partition with one intra-partition wire. Verify `SystemManifest::from_json()` parses it.
- [ ] 1.2 Emit Dome Plan via Composer — write a small test/script that loads the manifest and descriptors, calls `emit_plan()`, and writes the resulting `DomePlan` to `plans/composer-can-demo/plan.json`. Verify the emitted plan has the expected structure (1 partition, 1 wire, 2 processes).
- [ ] 1.3 Fix descriptor hash in can_source ABI table — update `descriptor_hash` in `examples/components/cpp/can_source/can_source.cpp` from zeroed to `18fa3eb2b9bcdef6c93f29a9869d2f7ffbc298ce809731e27c879a828c1b1205` (matching nxdc-generated hash). Verify descriptor validation passes.
- [ ] 1.4 Compile the manifest executable as a CMake target `nxd_composer_can_demo` and make it depend on `nxd_component_descriptors`.

## 2. Composer Validation

- [ ] 2.1 Write `tests/composer_can_demo_test.cpp` — a test that loads the manifest, loads descriptors, runs `validate_against_descriptors()`, `validate_intra_partition_direct_wiring()`, and `validate_source_name_wiring()`. Verify zero diagnostics.
- [ ] 2.2 Add the test to CMakeLists.txt — `add_test(NAME nxd_composer_can_demo ...)`. Run and verify it passes.

## 3. Execution via nexigen-run

- [ ] 3.1 Run `nexigen-run` with the emitted Dome Plan from step 1.2. Verify it spawns both processes, drives frame 1, and exits cleanly.
- [ ] 3.2 Start `nxd_can_generator` and `nxd_can_subscriber_helper` on `CAN_DEMO` bus alongside nexigen-run — verify they interoperate (the subscriber receives frames published by the generator during the nexigen-run frame).

## 4. Documentation

- [ ] 4.1 Write `plans/composer-can-demo/README.md` covering:
      - Manifest structure (component instances, partitions, wires, processes)
      - Descriptor hashes (how to generate and update)
      - Composer validation commands
      - Dome Plan emission
      - Execution via nexigen-run
      - Full end-to-end workflow from scratch