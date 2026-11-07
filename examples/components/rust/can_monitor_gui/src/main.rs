// CAN_monitor — Rust egui GUI for CAN bus monitoring.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use eframe::egui;
use egui::Color32;
use std::io::{BufReader, Read};
use std::process::{Command, Stdio};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

// ── Binary protocol ─────────────────────────────────────────────────────────
const CAN_BATCH_SIZE: usize = 392;

#[repr(C)]
#[derive(Clone, Debug)]
struct CanFrame {
    hw_time_ns: i64, can_id: u32, direction: u8, kind: u8,
    raw_dlc: u8, payload_length: u8, payload: [u8; 8],
}

#[repr(C)]
struct RawCanBatch {
    frames: [CanFrame; 16], count: u32, assigned_frame: u32,
}

#[derive(Clone, Debug)]
struct FrameRow {
    id: String, dlc: String, payload: String, direction: String, kind: String,
}

fn decode_batch(data: &[u8]) -> Vec<FrameRow> {
    let batch: &RawCanBatch = unsafe { &*(data.as_ptr() as *const RawCanBatch) };
    let count = batch.count.min(16);
    let mut rows = Vec::with_capacity(count as usize);
    for i in 0..count {
        let f = &batch.frames[i as usize];
        rows.push(FrameRow {
            id: format!("{:#05X}", f.can_id),
            dlc: format!("{}", f.raw_dlc),
            payload: f.payload[..f.payload_length as usize]
                .iter().map(|b| format!("{:02X}", b)).collect::<Vec<_>>().join(" "),
            direction: match f.direction { 0 => "RX".into(), 1 => "SELF".into(), _ => "?".into() },
            kind: match f.kind { 0 => "DATA".into(), 1 => "RTR".into(), 2 => "ERR".into(), 3 => "HB".into(), _ => "?".into() },
        });
    }
    rows
}

// ── Egui App ────────────────────────────────────────────────────────────────
struct CANMonitorApp {
    bus_name: String,
    frames: Vec<FrameRow>,
    status: String,
    status_color: Color32,
    rx: Option<mpsc::Receiver<Vec<FrameRow>>>,
    subscriber_started: bool,
}

impl CANMonitorApp {
    fn start_subscriber(&mut self) {
        let (tx, rx) = mpsc::channel();
        self.rx = Some(rx);
        self.subscriber_started = true;
        self.status = "Subscribing...".into();
        self.status_color = Color32::GRAY;

        let bus = self.bus_name.clone();
        thread::spawn(move || {
            // Try multiple paths for the subscriber helper
            let paths = [
                "nxd_can_subscriber_helper",           // in PATH
                "./build/nxd_can_subscriber_helper",   // project root
                "../build/nxd_can_subscriber_helper",  // from within examples/
            ];

            let child_result = paths.iter()
                .find_map(|p| Command::new(p).arg(&bus)
                    .stdout(Stdio::piped())
                    .stderr(Stdio::inherit())
                    .spawn()
                    .ok());

            let child = match child_result {
                Some(c) => c,
                None => {
                    eprintln!("[monitor] can't spawn subscriber helper");
                    eprintln!("[monitor] build with: cmake --build build --target nxd_can_subscriber_helper");
                    return;
                }
            };

            let stdout = match child.stdout {
                Some(s) => s,
                None => { eprintln!("[monitor] no stdout from helper"); return; }
            };

            let mut reader = BufReader::new(stdout);
            let mut header = [0u8; 4];
            let mut batch = vec![0u8; CAN_BATCH_SIZE];

            loop {
                if reader.read_exact(&mut header).is_err() { break; }
                let sz = (header[0] as u32) | ((header[1] as u32) << 8)
                    | ((header[2] as u32) << 16) | ((header[3] as u32) << 24);
                if sz as usize != CAN_BATCH_SIZE { break; }
                if reader.read_exact(&mut batch).is_err() { break; }
                let rows = decode_batch(&batch);
                if tx.send(rows).is_err() { break; }
            }
        });
    }
}

impl Default for CANMonitorApp {
    fn default() -> Self {
        Self {
            bus_name: "CAN_DEMO".into(),
            frames: Vec::new(),
            status: "Press Enter to subscribe".into(),
            status_color: Color32::GRAY,
            rx: None,
            subscriber_started: false,
        }
    }
}

impl eframe::App for CANMonitorApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Poll for new frames
        if let Some(ref rx) = self.rx {
            while let Ok(rows) = rx.try_recv() {
                for r in rows.into_iter().rev() {
                    self.frames.insert(0, r);
                }
                self.frames.truncate(10);
                self.status = "Receiving ✓".into();
                self.status_color = Color32::GREEN;
            }
        }

        // Start subscriber on first frame if not started
        if !self.subscriber_started {
            self.start_subscriber();
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.label("CAN Bus:");
                let resp = ui.add(egui::TextEdit::singleline(&mut self.bus_name)
                    .hint_text("CAN_DEMO"));
                if resp.lost_focus() && (ui.input(|i| i.key_pressed(egui::Key::Enter))
                    || ui.input(|i| i.key_pressed(egui::Key::Tab))) {
                    self.frames.clear();
                    self.start_subscriber();
                }
                // Status indicator (colored circle)
                let (_id, rect) = ui.allocate_space(egui::vec2(12.0, 12.0));
                ui.painter().circle_filled(rect.center(), 6.0, self.status_color);
                ui.label(&self.status);
            });

            ui.separator();

            // Table header
            ui.horizontal(|ui| {
                ui.strong("ID      ");
                ui.strong(" DLC");
                ui.strong("  Payload");
                ui.strong("         Dir");
                ui.strong(" Kind");
            });
            ui.separator();

            // Frame rows
            egui::ScrollArea::vertical()
                .auto_shrink([false, false])
                .show(ui, |ui| {
                    for frame in &self.frames {
                        ui.horizontal(|ui| {
                            ui.monospace(&frame.id);
                            ui.monospace(format!("{:>3}", frame.dlc));
                            ui.monospace(&frame.payload);
                            ui.monospace(format!("{:>4}", frame.direction));
                            ui.monospace(format!("{:>4}", frame.kind));
                        });
                    }
                    if self.frames.is_empty() {
                        ui.label("(no frames received yet)");
                    }
                });
        });

        // Repaint ~20 FPS
        ctx.request_repaint_after(Duration::from_millis(50));
    }
}

fn main() -> Result<(), eframe::Error> {
    eframe::run_native(
        "CAN Monitor — NexDome Demo",
        eframe::NativeOptions {
            viewport: egui::ViewportBuilder::default()
                .with_inner_size([720.0, 480.0])
                .with_min_inner_size([400.0, 200.0]),
            ..Default::default()
        },
        Box::new(|_cc| Ok(Box::new(CANMonitorApp::default()))),
    )
}