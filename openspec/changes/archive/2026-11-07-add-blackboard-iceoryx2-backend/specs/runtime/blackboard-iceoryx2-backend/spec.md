# Spec Delta

## Purpose

Provides a multi-process zero-copy Blackboard backend so that operator panels
and other separate-process tools can read and write the same versioned shared
state as the simulation host without copying data through sockets or pipes.

## ADDED Requirements

### Requirement: Blackboard supports an iceoryx2 backend for multi-process access

The Blackboard runtime SHALL provide an iceoryx2-based backend that exposes the
same C API as the headless backend and can be selected when processes need to
share Blackboard entries across address spaces.

#### Scenario: Separate panel process opens a Blackboard entry
- **WHEN** a simulation process creates `scenario/state` using the iceoryx2 backend
- **THEN** a panel process running in a different OS process can open the same
  entry by name using the same backend

#### Scenario: Headless backend remains the default for in-process tests
- **WHEN** a test creates a Blackboard without requesting the iceoryx2 backend
- **THEN** the headless in-process backend is used and no iceoryx2 services are
  started

### Requirement: Cross-process entry creation follows single-writer semantics

The iceoryx2 backend SHALL enforce that exactly one process creates each entry
as the writer; other processes SHALL open the same entry for reading only.

#### Scenario: Writer creates entry and reader opens it
- **WHEN** the simulation process creates `scenario/state`
- **THEN** a panel process can open `scenario/state` for reading, and an attempt
  by the panel process to create the same entry fails

### Requirement: Cross-process reads validate the entry layout hash

The iceoryx2 backend SHALL reject a read or open operation when the caller's
layout hash does not match the layout hash of the entry writer.

#### Scenario: Panel with mismatched layout hash is rejected
- **WHEN** a panel process opens `scenario/state` with a layout hash different
  from the writer's layout hash
- **THEN** the open operation fails with a layout-mismatch diagnostic and no
  value is read

### Requirement: Cross-process writes propagate revision, apply_frame_no, and generation

Every successful write on the iceoryx2 backend SHALL make the entry's
`revision`, `apply_frame_no`, and `generation` visible to readers in other
processes.

#### Scenario: Reader observes a writer's update
- **WHEN** the writer updates `scenario/state` at frame 100 with revision 7 and
  generation 3
- **THEN** a reader in another process reads revision 7, `apply_frame_no` 100,
  and generation 3

### Requirement: Cross-process writes emit BlackboardUpdate events

The iceoryx2 backend SHALL emit a `BlackboardUpdate` event for every successful
write, and remote readers SHALL be able to observe the latest update for an
entry.

#### Scenario: Recording process observes a remote BlackboardUpdate
- **WHEN** the writer updates `scenario/state`
- **THEN** a reader in another process calling `nxd_blackboard_get_update`
  receives a `BlackboardUpdate` whose entry name, revision, `apply_frame_no`,
  generation, and layout hash match the written entry

### Requirement: iceoryx2 backend hot path performs no heap allocation

After initialization, create, write, read, and `BlackboardUpdate` operations on
the iceoryx2 backend SHALL not allocate heap memory, acquire blocking locks, or
throw exceptions.

#### Scenario: Steady-state panel read is allocation-free
- **WHEN** the system is armed and a panel process repeatedly reads
  `scenario/state`
- **THEN** allocation instrumentation reports zero heap allocations in the
  Blackboard read path

### Requirement: iceoryx2 backend lifecycle matches the simulation lifecycle

The iceoryx2 backend SHALL initialize its services on first use and release
shared-memory resources when all participants in the process have stopped.
Stopping one reader process SHALL NOT destroy the entry for the writer or for
other readers.

#### Scenario: Panel closes mid-run
- **WHEN** the panel process terminates while the simulation process continues
  writing `scenario/state`
- **THEN** the simulation process continues writing successfully and a new
  panel process can reconnect and observe subsequent values
