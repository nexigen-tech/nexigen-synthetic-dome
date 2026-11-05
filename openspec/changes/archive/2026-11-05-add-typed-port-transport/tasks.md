# Tasks

## 1. ABI and SDK extensions

- [x] 1.1 Extend `sdk/rust/nxd-abi/src/lib.rs` and `sdk/rust/nxd-abi/nxd_abi.h` with `NxdPortSlot`, `NxdPortIoBundle`, and optional `step_io` callback, bumping ABI minor. Verify generated C header compiles with both C++ and Rust tests.
- [x] 1.2 Update `sdk/cpp/include/nxd/sdk.hpp` with `nxd::Port<T>` input view and `nxd::OutputLoan<T>` output loan wrappers. Verify the wrappers compile under `-fno-exceptions -fno-rtti`.
- [x] 1.3 Update `sdk/rust/nxd-sdk/src/lib.rs` with equivalent Rust `Port<T>` and `OutputLoan<T>` wrappers. Verify Rust clippy and tests pass.

## 2. Host port buffer allocation and validation

- [x] 2.1 Extend `host/include/nxd/host/descriptor.hpp` and `host/src/descriptor.cpp` to parse port contract and layout hash. Verify descriptor validation tests.
- [x] 2.2 Extend `host/include/nxd/host/component.hpp` and `host/src/component.cpp` to allocate input/output buffers per instance and invoke `step_io` when ports are declared. Verify legacy `step` plugins still pass existing tests.
- [x] 2.3 Add layout-hash validation in the host before arming; reject mismatches with `NXD_ERROR_ABI_MISMATCH`. Verify with a deliberately mismatched descriptor test.

## 3. Transport backends

- [x] 3.1 Create `runtime/transport` library with abstract `Transport`, `Publisher`, `Subscriber`, and `Loan` interfaces. Verify the interface headers compile.
- [x] 3.2 Implement `HeadlessTransport` using pre-allocated buffers and deterministic frame queues for in-process tests. Verify unit tests for publish/subscribe and source-name isolation.
- [x] 3.3 Implement `IceoryxTransport` for local multi-process graphs using iceoryx2 publishers/subscribers keyed by source name. Verify a minimal two-process sample exchange.
  - **Done:** iceoryx2 v0.10.0 is fetched via CMake FetchContent, linked as `iceoryx2-c::static-lib`, and `IceoryxTransport` creates IPC pub/sub services per source name. Covered by in-process smoke and forked multi-process tests.
- [ ] 3.4 Add transport lifecycle tests ensuring no allocation after arming and clean shutdown. Verify allocation instrumentation is zero.
  - **Status:** Deferred. The headless transport currently allocates on the cold path during publisher/subscriber creation; allocation-after-arm instrumentation will be added when the transport is integrated with the armed runtime.

## 4. Composer and LET updates

- [ ] 4.1 Extend `runtime/composer` manifest and plan schemas with `Wire` entries (source instance/port, source name, contract, layout hash, LET boundary flag). Verify Composer unit tests.
- [ ] 4.2 Add Composer validation for required ports, duplicate bindings, and compatible contract hashes. Verify positive and negative plan fixtures.
- [ ] 4.3 Extend `runtime/let` to mark cross-partition wires and record read time/write deadline in the Dome Plan. Verify LET boundary detection in Composer tests.
  - **Status:** Deferred. Composer plan wiring and LET boundary tasks are staged for a follow-up change once port I/O primitives are stable.

## 5. Headless harness port fixtures

- [x] 5.1 Extend `components/testing` headless harness to accept typed input fixtures and capture typed outputs. Verify a test that feeds one sample and reads one output sample.
- [x] 5.2 Add fixture layout validation and deterministic replay support. Verify mismatched fixture sizes are rejected and repeated runs match.
- [ ] 5.3 Add allocation-after-arm detection for port I/O paths. Verify a component that allocates while accessing ports fails the test.
  - **Status:** Deferred. Allocation detection will be wired once the transport integration defines the armed hot path.

## 6. Golden and integration tests

- [x] 6.1 Add C++ test component with one input and one output port using the new SDK. Verify headless harness drives it end-to-end.
- [x] 6.2 Add equivalent Rust test component and prove cross-language golden output matches the C++ component for identical input. Verify golden test passes.
- [x] 6.3 Create a multi-process walking-skeleton test with `IceoryxTransport` that runs one deterministic frame and compares hashes. Verify both runs match.
  - **Done:** `nxd_transport_iceoryx_fork_test` forks a publisher child and a subscriber parent, exchanges a deterministic `uint64_t`, and verifies payload and metadata.

## 7. Verification and documentation

- [x] 7.1 Run C++ Debug, Release, ASan, UBSan, TSan, and Clang builds; fix findings. Verify all builds pass.
- [x] 7.2 Run Rust debug/release builds and clippy checks. Verify no warnings from changed crates. (One pre-existing clippy warning remains in `nxdc`.)
- [x] 7.3 Update `docs/component-sdk.md` and `docs/component-abi.md` with port I/O examples and layout-hash rules. Verify documented snippets compile.

## Workflow follow-up

- Archive the change after implementation and review.
- Return to `add-virtual-can-skeleton` and resume apply.
