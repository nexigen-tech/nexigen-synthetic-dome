# Spec Delta

## ADDED Requirements

### Requirement: Host initializes shared nexigen Infrastructure before plugin execution
The host SHALL own exactly one `nexigen::app::Infrastructure` per process, initialize it before any plugin lifecycle function is called, and stop all components before shutting Infrastructure down.

#### Scenario: Two plugins share one Infrastructure
- **WHEN** two component plugins are loaded in the same process
- **THEN** both observe the same initialized Infrastructure and telemetry handles are valid for both

### Requirement: Host applies core::ThreadSpec to declared roles
The host SHALL validate declared thread roles against `core::ThreadSpec`, apply scheduling and affinity on the target thread before work begins, and treat required-role failures as arming failures.

#### Scenario: Required FIFO configuration fails
- **WHEN** a descriptor declares a required `fifo` thread role and the operating system rejects the scheduling policy
- **THEN** arming aborts and the host reports the thread role that could not be applied
