//! Code generation tests for C++23 POD, Rust `#[repr(C)]`, and field descriptors.

use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn contract(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(path)
}

fn tmp_dir() -> PathBuf {
    let dir = std::env::temp_dir().join(format!("nxdc-codegen-{}", std::process::id()));
    fs::create_dir_all(&dir).unwrap();
    dir
}

/// Builds a temporary Rust crate from generated source and runs a build check.
fn check_rust_compiles(name: &str, source: &str) {
    let dir = tmp_dir().join(format!("rust-{name}"));
    fs::create_dir_all(&dir).unwrap();
    fs::write(dir.join("lib.rs"), source).unwrap();
    fs::write(
        dir.join("Cargo.toml"),
        format!(
            "[package]\nname = \"{}_test\"\nversion = \"0.1.0\"\nedition = \"2021\"\n[lib]\npath = \"lib.rs\"\n",
            name.to_ascii_lowercase(),
        ),
    )
    .unwrap();

    let status = Command::new("cargo")
        .args([
            "build",
            "--manifest-path",
            dir.join("Cargo.toml").to_str().unwrap(),
        ])
        .status()
        .expect("cargo should be available");
    assert!(status.success(), "Rust compilation failed for {name}");
}

#[test]
fn sample_header_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/sample_header.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn can_frame_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/can_frame.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn nested_record_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/nested_record.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn can_batch_classical_fd_xl_instances() {
    let schemas =
        nxdc::load_concrete_contracts(&contract("valid/can_batch_template.yaml")).unwrap();
    assert_eq!(schemas.len(), 3);
    assert!(schemas.iter().any(|s| s.name == "RawCanBatchClassical"));
    assert!(schemas.iter().any(|s| s.name == "RawCanBatchFd"));
    assert!(schemas.iter().any(|s| s.name == "RawCanBatchXl"));

    // Each concrete schema should have literal array counts now.
    for schema in &schemas {
        assert!(schema.params.is_empty());
        assert!(schema.concrete.is_empty());
    }
}

#[test]
fn can_batch_rust_instances_compile() {
    let schemas =
        nxdc::load_concrete_contracts(&contract("valid/can_batch_template.yaml")).unwrap();
    for schema in &schemas {
        check_rust_compiles(&schema.name, &nxdc::gen::rust::emit(schema).unwrap());
    }
}

#[test]
fn duplicate_concrete_rejected() {
    let err =
        nxdc::load_concrete_contracts(&contract("invalid/duplicate_concrete.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate concrete instance name 'A'"),
        "expected duplicate concrete diagnostic, got: {text}"
    );
}

#[test]
fn out_of_range_param_rejected() {
    let err =
        nxdc::load_concrete_contracts(&contract("invalid/out_of_range_param.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("value 100 is above maximum 10"),
        "expected range diagnostic, got: {text}"
    );
}

#[test]
fn sample_header_cpp_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/sample_header.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    let header = nxdc::gen::cpp::emit(&schemas[0]).unwrap();

    let dir = tmp_dir().join("cpp");
    fs::create_dir_all(&dir).unwrap();
    let header_path = dir.join("SampleHeader.hpp");
    fs::write(&header_path, header).unwrap();

    let main_path = dir.join("main.cpp");
    fs::write(
        &main_path,
        "#include \"SampleHeader.hpp\"\nint main() { nxd::contracts::SampleHeader h{}; (void)h; return 0; }\n",
    )
    .unwrap();

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
            dir.join("test").to_str().unwrap(),
        ])
        .status()
        .expect("g++ should be available");
    assert!(status.success(), "C++ compilation failed");
}

#[test]
fn descriptor_emits_fields() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/can_frame.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    let json = nxdc::gen::descriptor::emit(&schemas[0]).unwrap();
    assert!(json.contains("\"contract\": \"RawCanFrame\""));
    assert!(json.contains("\"name\": \"payload\""));
    assert!(json.contains("\"enum_labels\""));
}

/// Builds a temporary Rust crate from generated source, runs it, and returns stdout.
fn run_rust_golden(name: &str, lib_source: &str, main_source: &str) -> String {
    let dir = tmp_dir().join(format!("rust-golden-{name}"));
    fs::create_dir_all(&dir).unwrap();
    fs::create_dir_all(dir.join("src")).unwrap();

    let package = name.to_ascii_lowercase();
    fs::write(dir.join("src/lib.rs"), lib_source).unwrap();
    fs::write(dir.join("src/main.rs"), main_source).unwrap();
    fs::write(
        dir.join("Cargo.toml"),
        format!("[package]\nname = \"{package}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n",),
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
    String::from_utf8(output.stdout)
        .expect("golden output is utf-8")
        .trim()
        .to_owned()
}

#[test]
fn can_batch_cpp_compiles() {
    let _schemas =
        nxdc::load_concrete_contracts(&contract("valid/can_batch_template.yaml")).unwrap();
    let dir = tmp_dir().join("cpp-can-batch");
    fs::create_dir_all(&dir).unwrap();

    // Emit all concrete headers into a directory.
    let out_dir = dir.join("include/nxd/contracts");
    fs::create_dir_all(&out_dir).unwrap();
    let source = contract("valid/can_batch_template.yaml");
    let status = std::process::Command::new("cargo")
        .args([
            "run",
            "-q",
            "-p",
            "nxdc",
            "--",
            "emit-cpp",
            source.to_str().unwrap(),
            out_dir.to_str().unwrap(),
        ])
        .status()
        .expect("cargo should be available");
    assert!(status.success(), "emit-cpp failed for RawCanBatch");

    let main_path = dir.join("main.cpp");
    fs::write(
        &main_path,
        r#"#include "nxd/contracts/RawCanBatchClassical.hpp"
int main() {
    nxd::contracts::RawCanBatchClassical b{};
    (void)b;
    return 0;
}
"#,
    )
    .unwrap();

    let status = std::process::Command::new("g++")
        .args([
            "-std=c++23",
            "-fno-exceptions",
            "-fno-rtti",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            out_dir
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .to_str()
                .unwrap(),
            main_path.to_str().unwrap(),
            "-o",
            dir.join("test").to_str().unwrap(),
        ])
        .status()
        .expect("g++ should be available");
    assert!(
        status.success(),
        "C++ compilation failed for RawCanBatch concrete instances"
    );
}

#[test]
fn can_heartbeat_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/can_heartbeat.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn video_frame_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/video_frame.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn video_heartbeat_rust_compiles() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/video_heartbeat.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn video_aggregate_errors_rust_compiles() {
    let schemas =
        nxdc::load_concrete_contracts(&contract("valid/video_aggregate_errors.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn can_aggregate_errors_rust_compiles() {
    let schemas =
        nxdc::load_concrete_contracts(&contract("valid/can_aggregate_errors.yaml")).unwrap();
    assert_eq!(schemas.len(), 1);
    check_rust_compiles(
        &schemas[0].name,
        &nxdc::gen::rust::emit(&schemas[0]).unwrap(),
    );
}

#[test]
fn signal_value_rust_golden() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/signal_value.yaml")).unwrap();
    let schema = schemas.iter().find(|s| s.name == "SignalValueU64").unwrap();
    let source = nxdc::gen::rust::emit(schema).unwrap();

    let main = r#"fn main() {
    let v = signalvalueu64::SignalValueU64 {
        value: 0x0102030405060708u64,
        quality: signalvalueu64::Quality_E2EError,
        origin: signalvalueu64::Origin_Replay,
        _pad0: [0; 6],
    };
    let bytes: [u8; 16] = unsafe { core::mem::transmute(v) };
    for b in bytes {
        print!("{:02x}", b);
    }
}
"#;

    let out = run_rust_golden("SignalValueU64", &source, main);
    assert_eq!(out, "08070605040302010204000000000000");
}

#[test]
fn signal_value_cpp_golden() {
    let schemas = nxdc::load_concrete_contracts(&contract("valid/signal_value.yaml")).unwrap();
    let schema = schemas.iter().find(|s| s.name == "SignalValueU64").unwrap();
    let header = nxdc::gen::cpp::emit(schema).unwrap();

    let dir = tmp_dir().join("cpp-signal-value");
    fs::create_dir_all(&dir).unwrap();
    let header_path = dir.join("SignalValueU64.hpp");
    fs::write(&header_path, header).unwrap();

    let main_path = dir.join("main.cpp");
    fs::write(
        &main_path,
        r#"#include "SignalValueU64.hpp"
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
int main() {
    nxd::contracts::SignalValueU64 v{};
    v.value = 0x0102030405060708ULL;
    v.quality = nxd::contracts::Quality::E2EError;
    v.origin = nxd::contracts::Origin::Replay;
    auto const* p = reinterpret_cast<std::uint8_t const*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(p[i]);
    }
    return 0;
}
"#,
    )
    .unwrap();

    let bin = dir.join("signal_value_golden");
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
    assert!(status.success(), "C++ golden compilation failed");

    let output = Command::new(&bin)
        .output()
        .expect("golden binary should run");
    assert!(output.status.success(), "C++ golden run failed");
    let out = String::from_utf8(output.stdout).expect("golden output is utf-8");
    assert_eq!(out.trim(), "08070605040302010204000000000000");
}

#[test]
fn deterministic_regeneration() {
    let schemas1 = nxdc::load_concrete_contracts(&contract("valid/sample_header.yaml")).unwrap();
    let schemas2 = nxdc::load_concrete_contracts(&contract("valid/sample_header.yaml")).unwrap();
    let cpp1 = nxdc::gen::cpp::emit(&schemas1[0]).unwrap();
    let cpp2 = nxdc::gen::cpp::emit(&schemas2[0]).unwrap();
    let rust1 = nxdc::gen::rust::emit(&schemas1[0]).unwrap();
    let rust2 = nxdc::gen::rust::emit(&schemas2[0]).unwrap();
    let desc1 = nxdc::gen::descriptor::emit(&schemas1[0]).unwrap();
    let desc2 = nxdc::gen::descriptor::emit(&schemas2[0]).unwrap();
    let (canon1, hash1) = nxdc::canonical::emit(&schemas1[0]).unwrap();
    let (canon2, hash2) = nxdc::canonical::emit(&schemas2[0]).unwrap();

    assert_eq!(cpp1, cpp2);
    assert_eq!(rust1, rust2);
    assert_eq!(desc1, desc2);
    assert_eq!(canon1, canon2);
    assert_eq!(hash1, hash2);
}
