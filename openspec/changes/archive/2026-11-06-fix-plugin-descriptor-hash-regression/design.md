# Design: Fix plugin descriptor hash regression

## Context

`nxd::host::load_plugin` validates every plugin against three sources of truth:

1. The ABI table layout (`abi_version`, `table_size`, `fingerprint`).
2. The component identity (`component_name`, `component_version`).
3. The descriptor hash (`descriptor_hash` field in the ABI table vs. the
   generated `.descriptor.hash` sidecar file).

After the walking-skeleton descriptors were updated (for example, the
`SignalValueU64` contract was added to several components), a number of example
and test plugins still embed the previous 32-byte descriptor hash. The host
rightly rejects them, which manifests as four failing tests:

- `nxd_host_plugin_load`
- `nxd_host_component_lifecycle`
- `nxd_host_harness`
- `nxd_host_panic_failure`

The affected plugins copied the same stale hash during early skeleton
development:

- `examples/components/cpp/counter`
- `examples/components/rust/counter`
- `tests/components/cpp/lifecycle`
- `tests/components/cpp/allocating`
- `tests/components/rust/alloc_counter`
- `tests/components/rust/panic_step`
- `tests/plugins/bad_plugin.cpp` (used as the base for the negative
  `bad_descriptor_hash` case)

Other skeleton plugins (`empty`, `virtual_can`, `can_monitor`, `virtual_video`,
`video_opengl`, `scenario_consumer`) already carry hashes that match their
current generated descriptors and pass validation.

## Goals / Non-Goals

**Goals:**

- Replace the stale hard-coded `descriptor_hash` in each affected plugin with
  the value from its current generated `.descriptor.hash` file.
- Keep the `bad_descriptor_hash` negative test effective.
- Restore a green `ctest` run in the default `build/` directory.

**Non-Goals:**

- Changing the ABI, the host validation logic, or any durable spec.
- Automating descriptor-hash generation (worthwhile follow-up, but out of scope
  for this stabilization fix).
- Adding CI or documentation beyond the OpenSpec change artifacts.

## Decisions

1. **Manual hash update from generated sidecars.**
   The fastest way to stabilize the skeleton is to copy the current SHA-256
   hashes from `build/generated/descriptors/*.descriptor.hash` into the plugin
   sources. This is a short-term fix; a later change can generate the hash at
   compile time or link time to prevent future drift.

2. **Preserve the negative case.**
   `tests/plugins/bad_plugin.cpp` is compiled multiple times with different
   preprocessor flags. The `BAD_DESCRIPTOR_HASH` variant must continue to use a
   hash that does not match the deterministic counter descriptor. We will update
   the base hash to the current deterministic-counter hash and leave the
   intentionally wrong variant unchanged.

3. **No Rust source regeneration.**
   Rust plugins already embed the hash as a const array; we update the array
   literals directly. No contract bindings change.

## Risks / Trade-offs

- **Manual copy error** → Mitigation: verify with `ctest` after the edit; the
  host hash check is exact, so any typo will fail a test immediately.
- **Future descriptor drift** → Mitigation: this change makes the problem
  visible again only if a descriptor changes and its plugin is forgotten. A
  follow-up change should automate hash injection.

## Open Questions

None. The generated sidecar files are the single source of truth for the
expected hashes.
