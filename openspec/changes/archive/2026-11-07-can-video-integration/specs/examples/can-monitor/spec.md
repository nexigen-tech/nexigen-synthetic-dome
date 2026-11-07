# Spec Delta

## Purpose

Provides a standalone Rust GUI application that subscribes to a named iceoryx2 CAN bus and displays the most recent 10 CAN frames in a live-updating table, serving as a reference for Rust+GUI+iceoryx2 integration for external developers.

## ADDED Requirements

### Requirement: CAN monitor subscribes by logical source name
The CAN monitor SHALL connect to an iceoryx2 CAN bus by logical source name and decode every `RawCanBatchClassical` sample received on that bus, maintaining a visible buffer of the most recent 10 individual CAN frames.

#### Scenario: Monitor receives and decodes CAN batches
- **WHEN** a CAN generator is publishing on bus `CAN_DEMO` and the monitor subscribes to `CAN_DEMO`
- **THEN** within two frame intervals the monitor SHALL display the decoded CAN frames

#### Scenario: Monitor shows only frames from the subscribed bus
- **WHEN** the monitor is subscribed to `CAN_DEMO`
- **THEN** frames published on a different bus (e.g., `OTHER_BUS`) SHALL NOT appear in the display

### Requirement: Bus name is configurable via GUI
The monitor SHALL provide a text input field in the GUI where the user can enter or change the iceoryx2 logical source name to subscribe to. When the field is changed, the monitor SHALL disconnect from the current bus and connect to the new one.

#### Scenario: User changes bus name in GUI
- **WHEN** the user types "DEMO_BUS_2" in the bus name field and presses Enter
- **THEN** the monitor disconnects from the previous bus
- **THEN** the monitor connects to iceoryx2 service `nxd/DEMO_BUS_2`
- **THEN** the frame display clears and begins showing frames from the new bus

### Requirement: Monitor displays last 10 CAN frames in a table
The monitor GUI SHALL display a scrollable table showing the 10 most recent individual CAN frames, with columns for CAN ID (hex), DLC, payload bytes (hex), direction (Rx/SelfRx), kind (Data/Remote/Error/Heartbeat), and a nanosecond timestamp. Frames SHALL be ordered with the most recent at the top.

#### Scenario: Frame table updates live
- **WHEN** a new CAN batch is received containing 3 frames
- **THEN** the table adds those 3 frames at the top
- **THEN** if the table now exceeds 10 rows, the oldest rows are removed

#### Scenario: Frame table shows correct content
- **WHEN** a received frame has can_id=0x100, dlc=4, payload=[0xDE,0xAD,0xBE,0xEF]
- **THEN** the table row shows "0x100" in the ID column, "4" in the DLC column, and "DE AD BE EF" in the payload column

### Requirement: Monitor displays connection status
The monitor GUI SHALL show a visual indicator (e.g., a coloured circle or status text) that reflects whether the monitor is connected to the iceoryx2 bus and actively receiving frames.

#### Scenario: Connected status indicator
- **WHEN** the monitor is receiving frames on the CAN bus
- **THEN** the status indicator SHALL show "Connected" (green)

#### Scenario: Disconnected status indicator
- **WHEN** no frames have been received for more than 10 ms
- **THEN** the status indicator SHALL show "No signal" (yellow)

#### Scenario: Invalid bus name indicator
- **WHEN** the user enters a bus name that does not exist or has no publishers
- **THEN** the status indicator SHALL show "No source" (red) within 50 ms

### Requirement: Monitor performs no heap allocation on the receive path
After the initial subscription setup, the CAN monitor SHALL not allocate heap memory on the iceoryx2 sample receive path.

#### Scenario: Receive loop is allocation-free
- **WHEN** the monitor has been running for 1 second and is actively receiving frames
- **THEN** no heap allocations occurred on the receive path after the first frame
