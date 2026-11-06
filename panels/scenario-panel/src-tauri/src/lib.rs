//! Tauri backend for the scenario panel.
//!
//! The backend is the only place that talks to the shared Blackboard/iceoryx2
//! backend.  The Svelte frontend receives only small decimated snapshots via
//! Tauri commands/events and sends allowlisted commands back through
//! `send_scenario_command`.

use serde::Serialize;

#[derive(Serialize)]
struct ScenarioSnapshot {
    value: u32,
    generation: u32,
    apply_frame_no: u64,
}

#[tauri::command]
fn send_scenario_command(command_id: u32, apply_frame_no: u64, value: u32) -> Result<String, String> {
    // TODO: publish a ScenarioCommand event to the shared Blackboard/transport
    // backend so that `scenario_control` applies it at `apply_frame_no`.
    Ok(format!(
        "command {} queued for frame {} with value {}",
        command_id, apply_frame_no, value
    ))
}

#[tauri::command]
fn latest_scenario_state() -> Result<ScenarioSnapshot, String> {
    // TODO: subscribe to `scenario/state` and return a decimated snapshot.
    Ok(ScenarioSnapshot {
        value: 0,
        generation: 0,
        apply_frame_no: 0,
    })
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![
            send_scenario_command,
            latest_scenario_state
        ])
        .run(tauri::generate_context!())
        .expect("error while running scenario panel");
}
