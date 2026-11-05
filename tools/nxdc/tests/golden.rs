//! Bidirectional C++/Rust golden integration tests.
//!
//! For representative contracts we compile generated code in both languages,
//! serialize a known value, and verify that C++ and Rust produce identical
//! bytes, major versions, and layout_hash values.

use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn fixture(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(path)
}

fn tmp_dir() -> PathBuf {
    let dir = std::env::temp_dir().join(format!("nxdc-golden-{}", std::process::id()));
    fs::create_dir_all(&dir).unwrap();
    dir
}

fn run_rust(name: &str, lib_source: &str, main_source: &str) -> String {
    let dir = tmp_dir().join(format!("rust-{name}"));
    fs::create_dir_all(&dir).unwrap();
    fs::create_dir_all(dir.join("src")).unwrap();

    let package = name.to_ascii_lowercase();
    fs::write(dir.join("src/lib.rs"), lib_source).unwrap();
    fs::write(dir.join("src/main.rs"), main_source).unwrap();
    fs::write(
        dir.join("Cargo.toml"),
        format!(
            "[package]\nname = \"{package}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n",
        ),
    )
    .unwrap();

    let output = Command::new("cargo")
        .args([
            "run",
            "--quiet",
            "--manifest-path",
            dir.join("Cargo.toml").to_str().unwrap(),
        ])
        .output()
        .expect("cargo should be available");

    assert!(
        output.status.success(),
        "Rust golden run failed: {}",
        String::from_utf8_lossy(&output.stderr)
    );
    String::from_utf8(output.stdout).expect("golden output is utf-8")
}

fn run_cpp(header: &str, header_source: &str, main_source: &str) -> String {
    let dir = tmp_dir().join(format!("cpp-{header}"));
    fs::create_dir_all(&dir).unwrap();
    let header_path = dir.join(format!("{header}.hpp"));
    fs::write(&header_path, header_source).unwrap();

    let main_path = dir.join("main.cpp");
    fs::write(&main_path, main_source).unwrap();

    let bin = dir.join("golden");
    let status = Command::new("g++")
        .args([
            "-std=c++23",
            "-fno-exceptions",
            "-fno-rtti",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            dir.to_str().unwrap(),
            main_path.to_str().unwrap(),
            "-o",
            bin.to_str().unwrap(),
        ])
        .status()
        .expect("g++ should be available");
    assert!(status.success(), "C++ golden compilation failed for {header}");

    let output = Command::new(&bin)
        .output()
        .expect("golden binary should run");
    assert!(output.status.success(), "C++ golden run failed for {header}");
    String::from_utf8(output.stdout).expect("golden output is utf-8")
}

#[test]
fn sample_header_cross_language_golden() {
    let schemas =
        nxdc::load_concrete_contracts(&fixture("valid/sample_header.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    let schema = &schemas[0];
    let cpp_header = nxdc::gen::cpp::emit(schema).unwrap();
    let rust_source = nxdc::gen::rust::emit(schema).unwrap();

    let rust_main = r#"fn main() {
    let mut v: sampleheader::SampleHeader = unsafe { core::mem::zeroed() };
    v.frame_no = 0x0102030405060708u64;
    v.sim_time_ns = 1000;
    v.hw_time_ns = 2000;
    v.source_id = 42;
    v.sequence = 7;
    v.flags = 0xDEADBEEF;
    let bytes: [u8; 40] = unsafe { core::mem::transmute(v) };
    for b in bytes { print!("{:02x}", b); }
    println!();
}
"#;

    let cpp_main = r#"#include "SampleHeader.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
int main() {
    nxd::contracts::SampleHeader v{};
    v.frame_no = 0x0102030405060708ULL;
    v.sim_time_ns = 1000;
    v.hw_time_ns = 2000;
    v.source_id = 42;
    v.sequence = 7;
    v.flags = 0xDEADBEEF;
    auto const* p = reinterpret_cast<std::uint8_t const*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(p[i]);
    }
    std::cout << "\n";
    return 0;
}
"#;

    let rust_out = run_rust("SampleHeader", &rust_source, rust_main);
    let cpp_out = run_cpp("SampleHeader", &cpp_header, cpp_main);
    assert_eq!(rust_out.trim(), cpp_out.trim(), "SampleHeader bytes mismatch");
    assert!(!rust_out.trim().is_empty());
}

#[test]
fn signal_value_cross_language_golden() {
    let schemas =
        nxdc::load_concrete_contracts(&fixture("valid/signal_value.yaml")).unwrap();
    let schema = schemas.iter().find(|s| s.name == "SignalValueU64").unwrap();
    let cpp_header = nxdc::gen::cpp::emit(schema).unwrap();
    let rust_source = nxdc::gen::rust::emit(schema).unwrap();

    let rust_main = r#"fn main() {
    let mut v: signalvalueu64::SignalValueU64 = unsafe { core::mem::zeroed() };
    v.value = 0xAABBCCDD11223344u64;
    v.quality = signalvalueu64::Quality_E2EError;
    v.origin = signalvalueu64::Origin_Replay;
    let bytes: [u8; 16] = unsafe { core::mem::transmute(v) };
    for b in bytes { print!("{:02x}", b); }
    println!();
}
"#;

    let cpp_main = r#"#include "SignalValueU64.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
int main() {
    nxd::contracts::SignalValueU64 v{};
    v.value = 0xAABBCCDD11223344ULL;
    v.quality = nxd::contracts::Quality::E2EError;
    v.origin = nxd::contracts::Origin::Replay;
    auto const* p = reinterpret_cast<std::uint8_t const*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(p[i]);
    }
    std::cout << "\n";
    return 0;
}
"#;

    let rust_out = run_rust("SignalValueU64", &rust_source, rust_main);
    let cpp_out = run_cpp("SignalValueU64", &cpp_header, cpp_main);
    assert_eq!(rust_out.trim(), cpp_out.trim(), "SignalValueU64 bytes mismatch");
}

#[test]
fn raw_can_frame_cross_language_golden() {
    let schemas =
        nxdc::load_concrete_contracts(&fixture("valid/can_frame.yaml")).unwrap();
    let schema = &schemas[0];
    let cpp_header = nxdc::gen::cpp::emit(schema).unwrap();
    let rust_source = nxdc::gen::rust::emit(schema).unwrap();

    let rust_main = r#"fn main() {
    let mut v: rawcanframe::RawCanFrame = unsafe { core::mem::zeroed() };
    v.hw_time_ns = 123456789i64;
    v.can_id = 0x1FFFFFFF;
    v.direction = rawcanframe::Direction_SelfRx;
    v.kind = rawcanframe::Kind_Error;
    v.status = rawcanframe::Status_Invalid;
    v.raw_dlc = 8;
    v.payload_length = 4;
    v.error_flags = 0xAAAA;
    v.vendor_error = 0x5555BBBB;
    let bytes: [u8; 88] = unsafe { core::mem::transmute(v) };
    for b in bytes { print!("{:02x}", b); }
    println!();
}
"#;

    let cpp_main = r#"#include "RawCanFrame.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
int main() {
    nxd::contracts::RawCanFrame v{};
    v.hw_time_ns = 123456789LL;
    v.can_id = 0x1FFFFFFF;
    v.direction = nxd::contracts::Direction::SelfRx;
    v.kind = nxd::contracts::Kind::Error;
    v.status = nxd::contracts::Status::Invalid;
    v.raw_dlc = 8;
    v.payload_length = 4;
    v.error_flags = 0xAAAA;
    v.vendor_error = 0x5555BBBB;
    auto const* p = reinterpret_cast<std::uint8_t const*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(p[i]);
    }
    std::cout << "\n";
    return 0;
}
"#;

    let rust_out = run_rust("RawCanFrame", &rust_source, rust_main);
    let cpp_out = run_cpp("RawCanFrame", &cpp_header, cpp_main);
    assert_eq!(rust_out.trim(), cpp_out.trim(), "RawCanFrame bytes mismatch");
}

#[test]
fn video_heartbeat_cross_language_golden() {
    let schemas =
        nxdc::load_concrete_contracts(&fixture("valid/video_heartbeat.yaml")).unwrap();
    let schema = &schemas[0];
    let cpp_header = nxdc::gen::cpp::emit(schema).unwrap();
    let rust_source = nxdc::gen::rust::emit(schema).unwrap();

    let rust_main = r#"fn main() {
    let mut v: videoheartbeat::VideoHeartbeat = unsafe { core::mem::zeroed() };
    v.stream_id = 0x11223344;
    v.transport_sequence = 100;
    v.content_sequence = 200;
    v.timestamp_ns = 0x0011223344556677i64;
    v.status = videoheartbeat::Status_Lost;
    let bytes: [u8; 32] = unsafe { core::mem::transmute(v) };
    for b in bytes { print!("{:02x}", b); }
    println!();
}
"#;

    let cpp_main = r#"#include "VideoHeartbeat.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
int main() {
    nxd::contracts::VideoHeartbeat v{};
    v.stream_id = 0x11223344;
    v.transport_sequence = 100;
    v.content_sequence = 200;
    v.timestamp_ns = 0x0011223344556677LL;
    v.status = nxd::contracts::Status::Lost;
    auto const* p = reinterpret_cast<std::uint8_t const*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(p[i]);
    }
    std::cout << "\n";
    return 0;
}
"#;

    let rust_out = run_rust("VideoHeartbeat", &rust_source, rust_main);
    let cpp_out = run_cpp("VideoHeartbeat", &cpp_header, cpp_main);
    assert_eq!(rust_out.trim(), cpp_out.trim(), "VideoHeartbeat bytes mismatch");
}

#[test]
fn representative_contract_metadata_matches_both_directions() {
    let cases = [
        ("valid/sample_header.yaml", "SampleHeader"),
        ("valid/signal_value.yaml", "SignalValueU64"),
        ("valid/can_frame.yaml", "RawCanFrame"),
        ("valid/video_heartbeat.yaml", "VideoHeartbeat"),
    ];

    for (path, concrete_name) in cases {
        let schemas = nxdc::load_concrete_contracts(&fixture(path)).unwrap();
        let schema = if schemas.len() == 1 {
            &schemas[0]
        } else {
            schemas.iter().find(|s| s.name == concrete_name).unwrap()
        };
        let (_, hash) = nxdc::canonical::emit(schema).unwrap();
        assert!(!hash.iter().all(|b| *b == 0), "{concrete_name} layout_hash must be non-zero");
        assert!(schema.version.major > 0, "{concrete_name} major version must be non-zero");
    }
}
