//! Canonical JSON and `layout_hash` determinism tests.

use std::fs;
use std::path::PathBuf;

use nxdc::model::{PrimitiveType, Type};

fn fixture(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(path)
}

fn load_fixture_text(path: &str) -> String {
    fs::read_to_string(fixture(path)).unwrap()
}

#[test]
fn sample_header_canonical_layout() {
    let schema = nxdc::load_contract(&fixture("valid/sample_header.yaml")).unwrap();
    let (layout, fields) = nxdc::canonical::root_layout(&schema).unwrap();

    assert_eq!(layout.alignment, 8);
    // frame_no:0, sim_time_ns:8, hw_time_ns:16, source_id:24, sequence:28, flags:32
    // trailing padding to 8 -> size 40
    assert_eq!(layout.size, 40);
    assert_eq!(fields.iter().find(|f| f.name == "frame_no").unwrap().offset, 0);
    assert_eq!(fields.iter().find(|f| f.name == "source_id").unwrap().offset, 24);
}

#[test]
fn canonical_hash_is_byte_identical_across_runs() {
    let schema = nxdc::load_contract(&fixture("valid/sample_header.yaml")).unwrap();
    let (_, hash1) = nxdc::canonical::emit(&schema).unwrap();
    let (_, hash2) = nxdc::canonical::emit(&schema).unwrap();
    assert_eq!(hash1, hash2);
}

#[test]
fn canonical_hash_does_not_depend_on_source_path() {
    let text = load_fixture_text("valid/sample_header.yaml");

    let dir = std::env::temp_dir().join("nxdc-canonical-test");
    fs::create_dir_all(&dir).unwrap();
    let path_a = dir.join("a.yaml");
    let path_b = dir.join("deep/b.yaml");
    fs::create_dir_all(path_b.parent().unwrap()).unwrap();
    fs::write(&path_a, &text).unwrap();
    fs::write(&path_b, &text).unwrap();

    let schema_a = nxdc::load_contract(&path_a).unwrap();
    let schema_b = nxdc::load_contract(&path_b).unwrap();
    let (_, hash_a) = nxdc::canonical::emit(&schema_a).unwrap();
    let (_, hash_b) = nxdc::canonical::emit(&schema_b).unwrap();
    assert_eq!(hash_a, hash_b);
}

#[test]
fn description_edit_preserves_hash() {
    let text = load_fixture_text("valid/sample_header.yaml");
    let edited = text.replace(
        "description: Common sample metadata for every transported sample.",
        "description: Completely different description.",
    );

    let dir = std::env::temp_dir().join("nxdc-desc-test");
    fs::create_dir_all(&dir).unwrap();
    let original_path = dir.join("orig.yaml");
    let edited_path = dir.join("edited.yaml");
    fs::write(&original_path, text).unwrap();
    fs::write(&edited_path, edited).unwrap();

    let schema_orig = nxdc::load_contract(&original_path).unwrap();
    let schema_edit = nxdc::load_contract(&edited_path).unwrap();
    let (_, hash_orig) = nxdc::canonical::emit(&schema_orig).unwrap();
    let (_, hash_edit) = nxdc::canonical::emit(&schema_edit).unwrap();
    assert_eq!(hash_orig, hash_edit);
}

#[test]
fn layout_edit_changes_hash() {
    let text = load_fixture_text("valid/sample_header.yaml");
    let edited = text.replace("type: u64\n    description: Logical frame number.", "type: u32\n    description: Logical frame number.");

    let dir = std::env::temp_dir().join("nxdc-layout-test");
    fs::create_dir_all(&dir).unwrap();
    let original_path = dir.join("orig.yaml");
    let edited_path = dir.join("edited.yaml");
    fs::write(&original_path, load_fixture_text("valid/sample_header.yaml")).unwrap();
    fs::write(&edited_path, edited).unwrap();

    let schema_orig = nxdc::load_contract(&original_path).unwrap();
    let schema_edit = nxdc::load_contract(&edited_path).unwrap();
    let (_, hash_orig) = nxdc::canonical::emit(&schema_orig).unwrap();
    let (_, hash_edit) = nxdc::canonical::emit(&schema_edit).unwrap();
    assert_ne!(hash_orig, hash_edit);
}

#[test]
fn wire_semantic_edit_changes_hash() {
    let text = load_fixture_text("valid/sample_header.yaml");
    let edited = text.replace(
        "  - name: frame_no\n    type: u64\n    description: Logical frame number.",
        "  - name: frame_no\n    type: u64\n    unit: frames\n    description: Logical frame number.",
    );

    let dir = std::env::temp_dir().join("nxdc-semantic-test");
    fs::create_dir_all(&dir).unwrap();
    let original_path = dir.join("orig.yaml");
    let edited_path = dir.join("edited.yaml");
    fs::write(&original_path, text).unwrap();
    fs::write(&edited_path, edited).unwrap();

    let schema_orig = nxdc::load_contract(&original_path).unwrap();
    let schema_edit = nxdc::load_contract(&edited_path).unwrap();
    let (_, hash_orig) = nxdc::canonical::emit(&schema_orig).unwrap();
    let (_, hash_edit) = nxdc::canonical::emit(&schema_edit).unwrap();
    assert_ne!(hash_orig, hash_edit);
}

#[test]
fn canonical_json_contains_format_identifier() {
    let schema = nxdc::load_contract(&fixture("valid/sample_header.yaml")).unwrap();
    let (json, _) = nxdc::canonical::emit(&schema).unwrap();
    assert!(json.contains("\"format\":\"nxd-layout-v1\""));
}

#[test]
fn can_batch_empty_watermark() {
    let schemas = nxdc::load_concrete_contracts(&fixture("valid/can_batch_template.yaml")).unwrap();
    let classical = schemas
        .iter()
        .find(|s| s.name == "RawCanBatchClassical")
        .unwrap();
    let (layout, fields) = nxdc::canonical::root_layout(classical).unwrap();
    assert_eq!(layout.size, 392);

    let count = fields.iter().find(|f| f.name == "count").unwrap();
    assert_eq!(count.ty, Type::Primitive(PrimitiveType::U32));
    assert_eq!(count.offset, 384);

    let assigned = fields.iter().find(|f| f.name == "assigned_frame").unwrap();
    assert_eq!(assigned.ty, Type::Primitive(PrimitiveType::U32));
    assert_eq!(assigned.offset, 388);
}

#[test]
fn can_batch_storage_sizes() {
    let schemas = nxdc::load_concrete_contracts(&fixture("valid/can_batch_template.yaml")).unwrap();
    let expected = [
        ("RawCanBatchClassical", 8usize, 392usize),
        ("RawCanBatchFd", 64usize, 2568usize),
        ("RawCanBatchXl", 2048usize, 16520usize),
    ];

    for (name, payload_size, total_size) in expected {
        let schema = schemas.iter().find(|s| s.name == name).unwrap();
        let (layout, _) = nxdc::canonical::root_layout(schema).unwrap();
        assert_eq!(layout.size, total_size, "{name} total size");

        let (json, _) = nxdc::canonical::emit(schema).unwrap();
        let value: serde_json::Value = serde_json::from_str(&json).unwrap();
        let types = value["types"].as_array().unwrap();
        let payload_field = types
            .iter()
            .find(|t| t["name"] == "CanFrame")
            .unwrap()["fields"]
            .as_array()
            .unwrap()
            .iter()
            .find(|f| f["name"] == "payload")
            .unwrap();
        assert_eq!(
            payload_field["size"].as_u64().unwrap() as usize,
            payload_size,
            "{name} payload field size"
        );
    }
}
