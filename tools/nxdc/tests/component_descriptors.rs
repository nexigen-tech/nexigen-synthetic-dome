//! Component descriptor parsing, validation, canonicalization, and CLI tests.

use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn fixture(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(path)
}

fn component(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("..")
        .join(path)
}

#[test]
fn empty_component_loads() {
    let descriptor = nxdc::load_component(&component("components/empty.yaml")).unwrap();
    assert_eq!(descriptor.name, "Empty");
    assert_eq!(descriptor.sdk.abi_major, 1);
    assert!(descriptor.ports.is_empty());
    assert!(descriptor.parameters.is_empty());
    assert!(descriptor.resources.deterministic);
}

#[test]
fn deterministic_counter_loads() {
    let descriptor = nxdc::load_component(&component("components/deterministic_counter.yaml")).unwrap();
    assert_eq!(descriptor.name, "DeterministicCounter");
    assert_eq!(descriptor.ports.len(), 2);
    assert!(descriptor.ports.iter().any(|p| p.name == "trigger"));
    assert!(descriptor.ports.iter().any(|p| p.name == "count_out"));
    assert_eq!(descriptor.parameters.len(), 2);
    assert!(descriptor.resources.uses_rng);
    assert_eq!(descriptor.resources.max_heap_bytes, Some(0));
}

#[test]
fn component_descriptor_hash_is_deterministic() {
    let descriptor = nxdc::load_component(&component("components/empty.yaml")).unwrap();
    let (json1, hash1) = nxdc::canonical::emit_component(&descriptor).unwrap();
    let (json2, hash2) = nxdc::canonical::emit_component(&descriptor).unwrap();
    assert_eq!(json1, json2);
    assert_eq!(hash1, hash2);
    assert!(!hash1.iter().all(|b| *b == 0));
}

#[test]
fn component_descriptor_excludes_hash_neutral_metadata() {
    let mut descriptor = nxdc::load_component(&component("components/empty.yaml")).unwrap();
    let (json1, hash1) = nxdc::canonical::emit_component(&descriptor).unwrap();
    descriptor.thread_roles.push(nxdc::model::ThreadRole {
        name: "ignored".to_owned(),
        required: false,
        single_cpu: false,
        default_schedule: None,
    });
    // Remove the added role so only the description-equivalent (not present) changes.
    descriptor.thread_roles.pop();
    let (json2, hash2) = nxdc::canonical::emit_component(&descriptor).unwrap();
    assert_eq!(json1, json2);
    assert_eq!(hash1, hash2);
}

#[test]
fn component_descriptor_includes_wire_semantics() {
    let mut descriptor = nxdc::load_component(&component("components/empty.yaml")).unwrap();
    let (_, hash1) = nxdc::canonical::emit_component(&descriptor).unwrap();
    descriptor.supported_modes.push("realtime".to_owned());
    let (_, hash2) = nxdc::canonical::emit_component(&descriptor).unwrap();
    assert_ne!(hash1, hash2);
}

#[test]
fn component_descriptor_contains_format_identifier() {
    let descriptor = nxdc::load_component(&component("components/empty.yaml")).unwrap();
    let (json, _) = nxdc::canonical::emit_component(&descriptor).unwrap();
    assert!(json.contains("\"format\":\"nxd-component-v1\""));
}

#[test]
fn zero_abi_major_rejected() {
    let err = nxdc::load_component(&fixture("invalid/component_zero_abi.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("SDK ABI major version must be non-zero"),
        "expected ABI major diagnostic, got: {text}"
    );
}

#[test]
fn duplicate_port_name_rejected() {
    let err = nxdc::load_component(&fixture("invalid/component_duplicate_port.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate port name"),
        "expected duplicate port diagnostic, got: {text}"
    );
}

#[test]
fn unbounded_parameter_type_rejected() {
    let err = nxdc::load_component(&fixture("invalid/component_unbounded_param.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("unbounded type 'string'"),
        "expected unbounded parameter diagnostic, got: {text}"
    );
}

#[test]
fn duplicate_parameter_name_rejected() {
    let err = nxdc::load_component(&fixture("invalid/component_duplicate_param.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate parameter name"),
        "expected duplicate parameter diagnostic, got: {text}"
    );
}

#[test]
fn invalid_component_descriptor_emits_no_partial_output() {
    let out_dir = PathBuf::from(std::env::var("TMPDIR").unwrap_or_else(|_| "/tmp".to_owned()))
        .join(format!("nxdc-component-cli-invalid-{}", std::process::id()));
    fs::create_dir_all(&out_dir).unwrap();
    let out_path = out_dir.join("bad.descriptor.json");

    let status = Command::new("cargo")
        .args([
            "run",
            "-q",
            "-p",
            "nxdc",
            "--",
            "emit-component-descriptor",
            fixture("invalid/component_zero_abi.yaml").to_str().unwrap(),
            out_path.to_str().unwrap(),
        ])
        .status()
        .expect("cargo should be available");
    assert!(!status.success(), "invalid descriptor should fail");
    assert!(!out_path.exists(), "partial output file must not exist");
}

#[test]
fn emit_component_descriptor_cli_works() {
    let out_dir = PathBuf::from(std::env::var("TMPDIR").unwrap_or_else(|_| "/tmp".to_owned()))
        .join(format!("nxdc-component-cli-{}", std::process::id()));
    fs::create_dir_all(&out_dir).unwrap();
    let out_path = out_dir.join("empty.descriptor.json");

    let status = Command::new("cargo")
        .args([
            "run",
            "-q",
            "-p",
            "nxdc",
            "--",
            "emit-component-descriptor",
            component("components/empty.yaml").to_str().unwrap(),
            out_path.to_str().unwrap(),
        ])
        .status()
        .expect("cargo should be available");
    assert!(status.success(), "emit-component-descriptor CLI failed");

    let text = fs::read_to_string(&out_path).unwrap();
    assert!(text.contains("\"format\":\"nxd-component-v1\""));
    assert!(text.contains("\"name\":\"Empty\""));
}
