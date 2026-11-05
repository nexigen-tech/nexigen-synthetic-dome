# NXD Component SDK

The Component SDK lets you write portable ABI-1 plugins in C++ or Rust.  A
plugin is a shared library that exports one symbol (`nxd_abi_entry`) returning
a versioned C ABI table.  The host loads the table, validates identity,
descriptor hash, and required resources, then drives the lifecycle.

## SDK layers

| Layer | C++ | Rust | Responsibility |
|-------|-----|------|----------------|
| ABI types | `sdk/cpp/include/nxd/abi.hpp` (wraps `nxd_abi.h`) | `sdk/rust/nxd-abi` | Fixed-layout `#[repr(C)]` structs, status codes, frame context |
| Safe wrappers | `sdk/cpp/include/nxd/sdk.hpp` | `sdk/rust/nxd-sdk` | Zero-allocation lifecycle helpers, state pointer casting, RNG wrapper |
| Examples | `examples/components/cpp/counter` | `examples/components/rust/counter` | Deterministic counter matching byte-for-byte |
| Test plugins | `tests/components/cpp/*` | `tests/components/rust/*` | Lifecycle, allocation, and panic injection |

## ABI-1 table

The table is the only thing that crosses the plugin boundary.  It must be
plain C-compatible data: function pointers, scalar version fields, a 64-byte
component name, and a 32-byte SHA-256 descriptor fingerprint.  No C++ objects,
no Rust owned types, no exceptions, and no unwinding may cross it.

Key invariants validated by the host before `initialize`:

* `abi_version.major` equals the descriptor's `sdk.abi_major`
* `table_size` equals `sizeof(NxdAbiTable)`
* `fingerprint` matches the ABI-1 target constant
* `component_name` and `component_version` match the descriptor
* `descriptor_hash` matches the generated sidecar `.hash` file
* All six lifecycle function pointers are non-null
* `extension` is null (reserved for future ABI versions)
* Required hardware-RT resources are not requested unless the host advertises
  them

## Lifecycle

The host guarantees this call order and reverse rollback:

1. `create`
2. `initialize`
3. `arm`
4. `step` (zero or more times)
5. `stop`
6. `destroy`

If `initialize` or `arm` fails, the host calls `destroy`.  If `step` fails,
the host calls `stop` then `destroy`.  The plugin must be ready for `destroy`
after any failed stage.

## Hot-path rules

After `arm` returns, the plugin enters the hot path:

* No heap allocation
* No locks
* No exceptions or unwinding
* No I/O, file reads, or dynamic dispatch
* No logging to stdout/stderr from `step`

Cold-path functions (`create`, `initialize`, `arm`, `stop`, `destroy`) may
allocate and use the C++/Rust standard library, but ABI-1 plugins are built
with `panic = "abort"` / `-fno-exceptions`, so errors are always returned as
`NxdStatus` codes.

## Building a C++ plugin

```cpp
#include "nxd/sdk.hpp"

struct MyState { /* plain POD only */ };

NxdComponentState* create() { return /* ... */; }
NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) { return NXD_OK; }
NxdStatus arm(NxdComponentState*, const NxdFrameContext*) { return NXD_OK; }
NxdStatus step(NxdComponentState*, const NxdFrameContext*) { return NXD_OK; }
void stop(NxdComponentState*) {}
void destroy(NxdComponentState*) {}

extern "C" const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    // fill abi_version, table_size, fingerprint, identity, descriptor_hash...
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.extension = nullptr;
    return &table;
}
```

Compile with `-fno-exceptions -fno-rtti` and link as a shared library.

## Building a Rust plugin

```rust
#![no_std]

use nxd_abi::{NxdAbiTable, NxdStatus, NXD_OK};
use nxd_sdk::{Component, FrameContext};

struct MyState;

impl Component for MyState {
    fn create() -> Result<Self, NxdStatus> { Ok(Self) }
    fn initialize(&mut self, _config: &str) -> Result<(), NxdStatus> { Ok(()) }
    fn arm(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> { Ok(()) }
    fn step(&mut self, _ctx: FrameContext<'_>) -> Result<(), NxdStatus> { Ok(()) }
}

#[no_mangle]
pub unsafe extern "C" fn nxd_abi_entry() -> *const NxdAbiTable {
    // build_abi_table fills the table and stores your descriptor_hash
}
```

`Cargo.toml`:

```toml
[lib]
crate-type = ["cdylib"]

[dependencies]
nxd-abi = { path = ".../sdk/rust/nxd-abi" }
nxd-sdk = { path = ".../sdk/rust/nxd-sdk" }
```

The workspace profile sets `panic = "abort"`.  A Rust panic must never unwind
across the C boundary; provide a `#[panic_handler]` that terminates the
process (for example `libc::abort`).

## Descriptor hash

`nxdc emit-component-descriptor <component.yaml> <out.json>` writes the
canonical descriptor and a sidecar `<out.json>.hash` containing the SHA-256
fingerprint.  The plugin hard-codes that 32-byte value in its ABI table.  Any
change to the component's name, version, ports, parameters, contracts,
resources, or thread roles changes the hash, so a mismatched plugin is
rejected before initialization.

## Thread roles

A descriptor may declare thread roles such as `main` with optional CPU
affinity and a scheduler policy (`other`, `fifo`, `rr`).  The host validates
every role as a `core::ThreadSpec` before arming.  In the headless harness all
roles are applied as best-effort: OS-level rejections are recorded in per-role
reports but do not prevent arming.  A syntactically invalid required role
prevents arming.

## Testing a component

Use `HeadlessHarness` to load, initialize, arm, and step a plugin without a
full simulation:

```cpp
nxd::host::HeadlessHarness harness(plugin_path, descriptor_path);
if (const auto st = harness.load()) {
    harness.initialize();
    harness.arm(seed);
    harness.run(frame_count);
}
```

Tests in `tests/` cover:

* ABI layout invariants (`nxd_abi_layout`)
* C++/Rust deterministic counter equivalence (`nxd_abi_plugin_equivalence`)
* Host validation of invalid plugins (`nxd_host_plugin_load`)
* Lifecycle rollback (`nxd_host_component_lifecycle`)
* Thread role validation and best-effort application (`nxd_host_thread_roles`)
* Headless harness for empty, counter, and allocating components
  (`nxd_host_harness`)
* Child-process panic and failure injection (`nxd_host_panic_failure`)
* Empty-component ABI overhead benchmark (`nxd_host_empty_benchmark`)

## Further reading

* `docs/contracts.md` — contract/schema compiler and IDL
* `docs/component-abi.md` — ABI ownership, lifetimes, status codes, versioning
