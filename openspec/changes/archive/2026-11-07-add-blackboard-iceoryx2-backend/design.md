# Design

## Context

- `runtime/blackboard` currently implements a headless in-process backend
  (`Blackboard` singleton + `Entry` seqlock).  It satisfies the durable spec's
  headless scenario and all existing tests.
- The durable `runtime/blackboard` spec already requires an iceoryx2 backend
  for multi-process panels; this backend does not exist yet.
- `runtime/transport` already uses `iceoryx2` C bindings for local multi-process
  transport, so the build system, dependency handling, and basic pub/sub
  patterns are proven in this repo.
- Components and the future panel link to the same `nxd_blackboard_*` C API.
  Any backend switch must keep that API and the in-memory `Entry` layout
  semantics unchanged.

See `proposal.md` for motivation and scope.

## Goals / Non-Goals

**Goals:**
- Provide an iceoryx2-backed Blackboard backend that works across OS processes.
- Keep the public C API unchanged so existing components and tests compile and
  behave identically.
- Make the headless backend the default; opt into iceoryx2 only when needed.
- Enforce single-writer-per-entry semantics across processes.
- Preserve layout-hash validation, revision/generation/`apply_frame_no`
  propagation, and `BlackboardUpdate` emission.
- Prove cross-process behavior with fork-based tests that can run in CI.

**Non-Goals:**
- Replacing or removing the headless backend.
- Changing Blackboard contracts or the `NxdBlackboardUpdate` layout.
- Implementing the Tauri panel backend (deferred to a follow-up change).
- Solving the TSan seqlock warning (deferred to `make-blackboard-seqlock-tsan-compatible`).
- Distributed transport over network; this stays local shared-memory only.

## Decisions

### 1. Backend selection via environment variable

- **Decision:** Select the backend through an environment variable
  `NXD_BLACKBOARD_BACKEND` with values `headless` (default) and `iceoryx2`.
- **Rationale:** Zero API changes for callers; tests keep using headless by
  default; panel and multi-process runs opt in explicitly.  A compile-time
  selection would force every build variant to link iceoryx2 even when unused.
- **Alternatives considered:**
  - Factory function parameter — rejected because it changes the public C API.
  - Auto-detection based on process count — rejected because it is implicit and
    complicates reproducibility.

### 2. One iceoryx2 service per Blackboard entry

- **Decision:** Each Blackboard entry maps to one iceoryx2 service.  The
  service name includes the entry's layout hash so that readers with a
  mismatched hash open a different service and fail immediately
  (e.g., `nxd_bb/entry/<hash>/<entry_name>`).  The writer owns the publisher;
  readers own subscribers.  A small fixed-size user header carries metadata
  (`revision`, `apply_frame_no`, `generation`, `layout_hash`).  The payload is
  the raw value bytes.
- **Rationale:** Matches the logical model of the headless backend (one entry
  = one named object) and reuses transport's proven "one service per logical
  source" pattern.  Embedding the layout hash in the service name gives
  immediate mismatch detection without an extra round-trip.  User header keeps
  metadata out of the value buffer, so callers still read exactly `value_size`
  bytes.
- **Alternatives considered:**
  - Single service with topics for all entries — rejected because it would
    require a custom multiplexing layer and complicate single-writer enforcement.
  - Metadata appended to payload — rejected because it changes the effective
    value size visible to callers.
  - A separate registry service for layout validation — rejected because the
    current iceoryx2 C-API has unreliable history delivery for that use case;
    the service-name encoding is simpler and deterministic.

### 3. Single-writer enforcement via a per-entry file lock

- **Decision:** Use an advisory `fcntl` writer lock on a per-entry file under
  `/tmp/opencode/nxd_bb_lock/<entry_name>.lock`.  `create_entry` acquires the
  lock before creating the iceoryx2 publisher; a second process fails the lock
  and receives `NXD_BLACKBOARD_ERROR_EXISTS`.  The lock is released when the
  writer handle is destroyed.
- **Rationale:** The current iceoryx2 C-API's `set_max_publishers` helper
  crashes the publisher builder in our environment, so we cannot rely on
  iceoryx2 to enforce a single publisher.  A per-entry `fcntl` lock is
  process-local, deterministic, and survives child processes without requiring
  an inter-process registry service.
- **Alternatives considered:**
  - Try-create publisher and rely on iceoryx2 failure for duplicate publishers
    — rejected because `set_max_publishers(1)` triggers a runtime mutex failure
    in the tested iceoryx2 version.
  - Single global mutex file — rejected because it would serialize all entry
    creation across the whole node.
  - Per-entry iceoryx2 event-service lock — rejected because dropping an
    inherited event notifier after `fork()` releases the parent process's lock.

### 4. C API implemented by a unified `nxd_blackboard` library

- **Decision:** Both backends live in the same `nxd_blackboard` library.  The
  public C functions dispatch to the active backend through a small vtable set
  during `nxd_blackboard_initialize`.
- **Rationale:** One link dependency for components and the panel; no ABI
  break; easy to add future backends.
- **Alternatives considered:**
  - Separate `nxd_blackboard_iceoryx2` library — rejected because it forces
    callers to choose at link time and complicates the SDK.

### 5. Lifecycle and cleanup

- **Decision:** `nxd_blackboard_initialize()` lazily creates the process-local
  iceoryx2 node.  If the current process is a child after `fork()`, the
  backend abandons inherited handles without dropping them (dropping in the
  child would release the parent's ports) and creates a fresh node.  Process
  exit triggers cleanup through `atexit`-registered destructors and explicit
  `stop` hooks in component lifecycle.  Stopping a reader process does not
  destroy shared services or writer endpoints.
- **Rationale:** Matches component lifecycle expectations and keeps the panel
  crash-safe: simulation continues even if the panel disappears.  Fork-safety
  is required because the cross-process tests use `fork()` and the global
  static backend state is copied to the child.
- **Alternatives considered:**
  - Reference-counted global cleanup — rejected because cross-process refcount
    is unreliable without an external supervisor.

## Risks / Trade-offs

- **[Risk] iceoryx2 C API verbosity and error handling** → Mitigation: wrap
  calls in small internal helpers with status enums; reuse patterns from
  `runtime/transport/src/iceoryx_transport.cpp`.
- **[Risk] Single-writer registry races during concurrent creation** →
  Mitigation: registry updates use atomic compare-and-swap style checks on the
  writer record; tests cover race windows.
- **[Risk] Resource leaks if a process crashes** → Mitigation: accept that
  iceoryx2 service names may remain until the underlying iceoryx2 runtime
  cleans them up; document that users should restart the iceoryx2 runtime
  between crashed runs in development.
- **[Risk] Sanitizer builds (TSan) flag iceoryx2 internals or shared-memory
  fences** → Mitigation: keep the existing `-Wno-tsan` suppression for the
  blackboard library until the dedicated seqlock change lands; add tests that
  run under ASan/UBSan.
- **[Trade-off] Environment variable is global per process** → A process cannot
  mix backends.  This is acceptable because headless tests never need
  multi-process access and panels always need iceoryx2.

## Open Questions

- None.  The approach is deferrable downstream work (panel wiring, launcher,
  seqlock TSan fix, CI workaround removal) is explicitly out of scope.
