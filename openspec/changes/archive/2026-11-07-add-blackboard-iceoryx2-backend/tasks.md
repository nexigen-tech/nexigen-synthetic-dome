# Tasks

## 1. Backend scaffolding and selection

- [x] 1.1 Introduce a backend abstraction (vtable or small abstract class) inside
  `runtime/blackboard/` so the public C API can dispatch to headless or iceoryx2.
  Verify by building `libnxd_blackboard` and running existing headless
  `nxd_blackboard_layout` tests unchanged.
- [x] 1.2 Add `NXD_BLACKBOARD_BACKEND` environment variable parsing with values
  `headless` (default) and `iceoryx2`. Verify that unset, `headless`, and
  invalid values all select the headless backend.

## 2. iceoryx2 backend implementation

- [x] 2.1 Create the iceoryx2 node on `nxd_blackboard_initialize()` and release
  it on process stop. Verify with new tests that initialization succeeds and
  cleanup does not crash.
- [x] 2.2 Implement `create_entry` with a per-entry POSIX writer lock to enforce
  a single writer per entry across processes. Verify that a second writer
  receives `NXD_BLACKBOARD_ERROR_EXISTS`.
- [x] 2.3 Implement `open_entry` for readers; validate the layout hash by
  including it in the iceoryx2 service name so a mismatched hash fails to open
  the entry.
- [x] 2.4 Implement `write` with a user header carrying `revision`,
  `apply_frame_no`, `generation`, and `layout_hash`. Verify that the writer
  side returns a new revision and emits a `BlackboardUpdate`.
- [x] 2.5 Implement `read` and `get_update` for subscribers, reconstructing
  metadata from the user header. Verify that a subscriber observes the same
  values as the headless backend.

## 3. Cross-process integration tests

- [x] 3.1 Add a fork-based test where the parent process writes an entry and a
  child process reads it. Verify that the child sees the expected value,
  revision, `apply_frame_no`, and generation.
- [x] 3.2 Add a fork-based test proving that closing the reader process does not
  affect the writer. Verify that the writer continues creating revisions and
  a new child can reconnect.
- [x] 3.3 Add a fork-based layout-mismatch test. Verify that the child fails to
  open the entry with a different layout hash.
- [x] 3.4 Register all new tests in CTest and verify they pass in default and
  Release builds.

## 4. Build and sanitizer compatibility

- [x] 4.1 Update `runtime/blackboard/CMakeLists.txt` to compile iceoryx2 backend
  sources and link `iceoryx2-c`. Verify that all 7 CMake variants configure
  without errors.
- [x] 4.2 Keep the existing `-Wno-tsan` suppression scoped to the blackboard
  library. Verify that `build-tsan` still passes its relevant test set.
- [x] 4.3 Run ASan and UBSan builds and confirm no new failures from the
  iceoryx2 backend or its tests.

## 5. Documentation and spec sync

- [x] 5.1 Update `runtime/blackboard` header documentation to describe backend
  selection and multi-process behavior.
- [x] 5.2 Run `openspec validate add-blackboard-iceoryx2-backend` and fix any
  spec/design/task inconsistencies.

## Workflow follow-up

- Archive the change after review and when all tracked tasks are complete.
- Verify the archived result.
