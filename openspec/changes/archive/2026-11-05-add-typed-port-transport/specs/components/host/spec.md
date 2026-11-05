# Spec Delta

## ADDED Requirements

### Requirement: Host validates port bindings before arming
The host SHALL validate that every declared port has a compatible contract binding, that input ports are connected to logical sources, and that output ports declare their source name, before calling the component `arm` callback.

#### Scenario: Required port without binding aborts arming
- **WHEN** a descriptor declares a required input port but Composer provides no source name
- **THEN** the host rejects arming and reports the unbound port

#### Scenario: Incompatible contract hash aborts arming
- **WHEN** a port binding references a contract whose `layout_hash` differs from the plugin's generated type
- **THEN** the host rejects arming with an ABI mismatch status

### Requirement: Host passes typed port buffers to the step callback
The host SHALL present input port buffers as read-only typed views and output port buffers as writable typed loans to the component step callback.

#### Scenario: Port-aware step receives input and output loans
- **WHEN** a plugin exports the extended step callback and declares input/output ports
- **THEN** the host invokes the callback with valid input views and output loans for every armed step

### Requirement: Host keeps port buffers alive only for the duration of the step
The host SHALL own all port buffer memory and SHALL ensure that no view or loan outlives the step callback.

#### Scenario: Retained output loan is detected
- **WHEN** a development test component retains an output loan pointer after step returns
- **THEN** the host or SDK detects the violation and fails the test
