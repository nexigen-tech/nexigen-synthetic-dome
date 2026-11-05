# Logical Execution Time (LET) Rules

This document describes the LET rules enforced by the NXD Composer and
runtime.  Every rule listed here has a corresponding Composer validation
scenario and a runtime unit test.

## R1. Direct communication is allowed only inside a partition

Components that share a partition may be wired directly.  A direct wire is one
whose `Wire::let_boundary_id` is empty.

### Enforcement

The Composer rejects any direct wire whose source and target instances belong
to different partitions, or whose target instance is not in any partition.

### Scenario

A manifest wires `pid_1.command` directly to `plant_1.setpoint`.  `pid_1` is in
partition `control`; `plant_1` is in partition `plant`.  Composer emits a
diagnostic naming both ports and partitions.

### Test

- `runtime/composer/tests/validation_test.cpp` — cross-partition direct wire
  rejected; orphan target rejected.

## R2. Cross-partition traffic must cross an explicit LET boundary

When a wire connects ports in different partitions, it must declare a
`let_boundary_id` that references a `LetBoundary` in the Dome Plan.

### Enforcement

For every wire whose source and target instances are in different partitions,
the Composer verifies that `let_boundary_id` is present and non-empty.

### Scenario

A manifest wires `pid_1.command` to `plant_1.setpoint` with
`let_boundary_id = "control_to_plant"`.  The Composer accepts the wire.

### Test

- `runtime/composer/tests/validation_test.cpp` — cross-partition wire with LET
  boundary accepted.

## R3. LET boundary declarations must match wire endpoints

A referenced LET boundary must exist and its source/target instance and port
names must exactly match the wire it serves.

### Enforcement

The Composer follows each `let_boundary_id` to its `LetBoundary` record and
compares `(source_instance, source_port, target_instance, target_port)`.

### Scenario

A manifest references boundary `"control_to_plant"` whose
`source_port` is `"alt_command"`, but the wire declares `"command"`.  The
Composer emits a boundary-mismatch diagnostic.

### Test

- `runtime/composer/tests/validation_test.cpp` — unknown boundary id rejected;
  endpoint mismatch rejected.

## R4. Writes to LET-bound ports must meet the declared deadline

At runtime, Chronos records the simulation time at which a component writes to a
cross-partition port and compares it to the boundary's `write_deadline`.  A
write at a simulation time greater than the deadline is recorded as a violation.

### Enforcement

`LetDeadlineTracker::record_write` looks up the boundary by endpoint tuple.  If
the write time exceeds `write_deadline`, a `LetViolation` is appended and
returned to the caller.

### Scenario

Boundary `"control_to_plant"` has `write_deadline_ns = 200000`.  A late write
at `SimTime{300000}` produces a violation with `lateness_ns = 100000`.  An
on-time write at `SimTime{150000}` produces no violation.

### Test

- `runtime/chronos/tests/let_boundary_test.cpp` — late write detected, on-time
  write accepted, intra-partition write ignored.

## R5. Intra-partition writes are not subject to LET deadlines

Only cross-partition ports with a declared boundary are tracked.  Writes to
ports whose endpoints are not covered by a LET boundary are silently ignored by
the deadline tracker.

### Enforcement

`LetDeadlineTracker::record_write` returns `std::nullopt` when no matching
boundary is found.

### Scenario

A write from `pid_1.command` to `mixer_1.input` inside the same partition is
recorded, but no violation is produced even if it occurs after an arbitrary
deadline, because no boundary was declared for that endpoint pair.

### Test

- `runtime/chronos/tests/let_boundary_test.cpp` — intra-partition write after
  deadline does not create a violation.

## Glossary

- **Partition**: a group of component instances that share the same frame
  context and may communicate directly.
- **LET boundary**: a declared coupling between two ports in different
  partitions, with a published `read_time` and a `write_deadline`.
- **Direct wire**: a wire with no `let_boundary_id`, permitted only inside a
  partition.
- **Violation**: a runtime record indicating that a write occurred after its
  LET boundary's `write_deadline`.
