//! Fixture-based parser and validation tests for `nxdc`.
//!
//! Valid fixtures must load into the validated intermediate model.  Invalid
//! fixtures must fail with field-specific diagnostics that mention the
//! offending path or type.

use std::path::PathBuf;

fn fixture(path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(path)
}

#[test]
fn sample_header_loads() {
    let schema = nxdc::load_contract(&fixture("valid/sample_header.yaml")).unwrap();
    assert_eq!(schema.name, "SampleHeader");
    assert_eq!(schema.version.to_string(), "1.0.0");
    assert_eq!(schema.fields.len(), 6);
    assert!(schema.types.is_empty());
}

#[test]
fn nested_record_loads() {
    let schema = nxdc::load_contract(&fixture("valid/nested_record.yaml")).unwrap();
    assert_eq!(schema.name, "NestedRecord");
    assert_eq!(schema.version.to_string(), "2.1.0");
    assert_eq!(schema.types.len(), 3);
    assert_eq!(schema.fields.len(), 2);

    let history = schema.fields.iter().find(|f| f.name == "history").unwrap();
    assert_eq!(history.ty.array_count(), Some(8));
    assert_eq!(history.ty.array_element().unwrap(), &nxdc::model::Type::Primitive(nxdc::model::PrimitiveType::F32));
}

#[test]
fn can_frame_loads() {
    let schema = nxdc::load_contract(&fixture("valid/can_frame.yaml")).unwrap();
    assert_eq!(schema.name, "RawCanFrame");
    assert_eq!(schema.types.len(), 3);
    let payload = schema.fields.iter().find(|f| f.name == "payload").unwrap();
    assert_eq!(payload.ty.array_count(), Some(64));
}

#[test]
fn field_metadata_loads() {
    let schema = nxdc::load_contract(&fixture("valid/field_metadata.yaml")).unwrap();
    assert_eq!(schema.name, "FieldMetadata");
    let temp = schema.fields.iter().find(|f| f.name == "temperature").unwrap();
    assert_eq!(temp.ty, nxdc::model::Type::Primitive(nxdc::model::PrimitiveType::F32));
    assert_eq!(temp.unit.as_deref(), Some("degC"));
    assert_eq!(temp.min, Some(-40));
    assert_eq!(temp.max, Some(150));
}

#[test]
fn component_basic_loads() {
    let desc = nxdc::load_component(&fixture("valid/component_basic.yaml")).unwrap();
    assert_eq!(desc.name, "can-gateway");
    assert_eq!(desc.sdk.abi_major, 1);
    assert_eq!(desc.ports.len(), 2);
    assert!(desc.ports.iter().any(|p| p.name == "can_in"));
    assert!(desc.resources.deterministic);
}

#[test]
fn unbounded_string_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/unbounded_string.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("unbounded type 'string'"),
        "expected unbounded diagnostic, got: {text}"
    );
    assert!(text.contains("fields[0].type"), "expected path in diagnostic: {text}");
}

#[test]
fn unbounded_vec_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/unbounded_vec.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("unbounded") && text.contains("'Vec<u8>'"),
        "expected unbounded diagnostic, got: {text}"
    );
}

#[test]
fn unbounded_bool_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/unbounded_bool.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("unbounded type 'bool'"),
        "expected unbounded diagnostic, got: {text}"
    );
}

#[test]
fn malformed_type_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/malformed_type.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("malformed type 'f128'"),
        "expected malformed type diagnostic, got: {text}"
    );
}

#[test]
fn malformed_array_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/malformed_array.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("malformed array count 'zero'"),
        "expected malformed array diagnostic, got: {text}"
    );
}

#[test]
fn duplicate_field_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/duplicate_field.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate field name 'value'"),
        "expected duplicate field diagnostic, got: {text}"
    );
}

#[test]
fn duplicate_type_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/duplicate_type.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate type name 'Status'"),
        "expected duplicate type diagnostic, got: {text}"
    );
}

#[test]
fn invalid_version_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/invalid_version.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("invalid version 'not.a.version'"),
        "expected invalid version diagnostic, got: {text}"
    );
}

#[test]
fn unknown_type_rejected() {
    let err = nxdc::load_contract(&fixture("invalid/unknown_type.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("unknown type reference 'NonExistent'"),
        "expected unknown type diagnostic, got: {text}"
    );
}

#[test]
fn signal_value_loads() {
    let schema = nxdc::load_contract(&fixture("valid/signal_value.yaml")).unwrap();
    assert_eq!(schema.name, "SignalValue");
    assert_eq!(schema.params.len(), 1);
    assert!(matches!(
        &schema.params[0].kind,
        nxdc::model::TemplateParamKind::TypeParam { .. }
    ));
    assert_eq!(schema.fields.len(), 3);
}

#[test]
fn can_payload_too_large_rejected() {
    let err = nxdc::load_concrete_contracts(&fixture("invalid/can_payload_too_large.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("value 4096 is above maximum 2048"),
        "expected max_payload range diagnostic, got: {text}"
    );
}

#[test]
fn can_capacity_too_large_rejected() {
    let err = nxdc::load_concrete_contracts(&fixture("invalid/can_capacity_too_large.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("value 512 is above maximum 256"),
        "expected batch_capacity range diagnostic, got: {text}"
    );
}

#[test]
fn can_heartbeat_loads() {
    let schema = nxdc::load_contract(&fixture("valid/can_heartbeat.yaml")).unwrap();
    assert_eq!(schema.name, "CanHeartbeat");
    assert!(schema.fields.iter().any(|f| f.name == "hw_time_ns"));
    assert!(schema.fields.iter().any(|f| f.name == "direction"));
    assert!(schema.fields.iter().any(|f| f.name == "kind"));
}

#[test]
fn can_aggregate_errors_loads() {
    let schema = nxdc::load_contract(&fixture("valid/can_aggregate_errors.yaml")).unwrap();
    assert_eq!(schema.name, "CanAggregateErrors");
    assert!(schema.fields.iter().any(|f| f.name == "detection_time_ns"));
    assert!(schema.fields.iter().any(|f| f.name == "heartbeat_time_ns"));
    assert!(schema.fields.iter().any(|f| f.name == "crc_errors"));
    assert!(schema.fields.iter().any(|f| f.name == "self_rx_count"));
}

#[test]
fn video_frame_loads() {
    let schema = nxdc::load_contract(&fixture("valid/video_frame.yaml")).unwrap();
    assert_eq!(schema.name, "VideoFrame");
    assert!(schema.fields.iter().any(|f| f.name == "codec"));
    assert!(schema.fields.iter().any(|f| f.name == "plane_offsets"));
    assert!(schema.fields.iter().any(|f| f.name == "payload"));
    assert!(schema.fields.iter().any(|f| f.name == "extension_id"));
}

#[test]
fn video_heartbeat_loads() {
    let schema = nxdc::load_contract(&fixture("valid/video_heartbeat.yaml")).unwrap();
    assert_eq!(schema.name, "VideoHeartbeat");
    assert!(schema.fields.iter().any(|f| f.name == "transport_sequence"));
    assert!(schema.fields.iter().any(|f| f.name == "content_sequence"));
    assert!(!schema.fields.iter().any(|f| f.name == "payload"));
}

#[test]
fn video_aggregate_errors_loads() {
    let schema = nxdc::load_contract(&fixture("valid/video_aggregate_errors.yaml")).unwrap();
    assert_eq!(schema.name, "VideoAggregateErrors");
    assert!(schema.fields.iter().any(|f| f.name == "missing_lines"));
    assert!(schema.fields.iter().any(|f| f.name == "crc_errors"));
}

#[test]
fn duplicate_port_rejected() {
    let err = nxdc::load_component(&fixture("invalid/duplicate_port.yaml")).unwrap_err();
    let text = err.to_string();
    assert!(
        text.contains("duplicate port name 'can'"),
        "expected duplicate port diagnostic, got: {text}"
    );
}
