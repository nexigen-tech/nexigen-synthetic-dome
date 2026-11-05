//! Canonical JSON and SHA-256 `layout_hash` generation.
//!
//! The canonical document captures everything that affects the wire layout
//! and interpretation of a contract, while excluding descriptions, source
//! paths, and other hash-neutral metadata.  It is emitted as deterministic
//! UTF-8 JSON and hashed with SHA-256 to produce the 32-byte `layout_hash`.

use crate::model::{
    ArrayCount, ComponentDescriptor, ContractRef, ContractSchema, EnumDef, EnumValue, Field,
    Parameter, Port, PortDirection, PortSemantics, PrimitiveType, RecordDef, Resources, ThreadRole,
    Type, TypeDef, Version,
};
use serde_json::{Map, Value};
use sha2::{Digest, Sha256};
use std::collections::HashMap;

/// Format identifier baked into every canonical document.
pub const CANONICAL_FORMAT: &str = "nxd-layout-v1";

/// Computed layout for a type.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Layout {
    pub size: usize,
    pub alignment: usize,
}

/// Computed layout for one field inside a record.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FieldLayout {
    pub name: String,
    pub ty: Type,
    pub offset: usize,
    pub size: usize,
    pub alignment: usize,
}

/// A canonicalization error.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanonicalError {
    pub message: String,
}

impl CanonicalError {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl std::fmt::Display for CanonicalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for CanonicalError {}

/// Emits the canonical UTF-8 JSON document and the 32-byte SHA-256 layout hash.
pub fn emit(schema: &ContractSchema) -> Result<(String, [u8; 32]), CanonicalError> {
    let value = schema_to_value(schema)?;
    let json = json_to_string(&value)?;
    let hash = sha256(&json);
    Ok((json, hash))
}

/// Computes the SHA-256 digest of a UTF-8 string.
pub fn sha256(text: &str) -> [u8; 32] {
    let mut hasher = Sha256::new();
    hasher.update(text.as_bytes());
    hasher.finalize().into()
}

fn json_to_string(value: &Value) -> Result<String, CanonicalError> {
    serde_json::to_string(value).map_err(|e| CanonicalError::new(format!("JSON serialization: {e}")))
}

fn schema_to_value(schema: &ContractSchema) -> Result<Value, CanonicalError> {
    let registry = build_registry(schema);
    let mut cache = HashMap::new();

    let mut root_fields = Vec::new();
    let mut offset = 0usize;
    let mut root_align = 1usize;
    for field in &schema.fields {
        let layout = type_layout(&field.ty, &registry, &mut cache)?;
        root_align = root_align.max(layout.alignment);
        offset = align_to(offset, layout.alignment);
        root_fields.push(field_layout_value(field, offset, layout)?);
        offset += layout.size;
    }
    let root_size = align_to(offset, root_align);

    let mut types = Vec::new();
    for td in &schema.types {
        types.push(type_def_value(td, &registry, &mut cache)?);
    }

    let mut obj = Map::new();
    obj.insert("alignment".to_owned(), Value::Number(root_align.into()));
    obj.insert("fields".to_owned(), Value::Array(root_fields));
    obj.insert("format".to_owned(), Value::String(CANONICAL_FORMAT.to_owned()));
    obj.insert("name".to_owned(), Value::String(schema.name.clone()));
    obj.insert("size".to_owned(), Value::Number(root_size.into()));
    obj.insert("types".to_owned(), Value::Array(types));
    obj.insert("version".to_owned(), version_value(schema.version));
    Ok(Value::Object(obj))
}

fn type_def_value(
    td: &TypeDef,
    registry: &HashMap<String, TypeDef>,
    cache: &mut HashMap<String, Layout>,
) -> Result<Value, CanonicalError> {
    match td {
        TypeDef::Record(rec) => record_value(rec, registry, cache),
        TypeDef::Enum(en) => enum_value(en),
    }
}

fn record_value(
    rec: &RecordDef,
    registry: &HashMap<String, TypeDef>,
    cache: &mut HashMap<String, Layout>,
) -> Result<Value, CanonicalError> {
    let (layout, field_layouts) = record_layout(&rec.fields, registry, cache)?;

    let mut fields = Vec::new();
    for fl in field_layouts {
        fields.push(field_layout_value_from(fl)?);
    }

    let mut obj = Map::new();
    obj.insert("alignment".to_owned(), Value::Number(layout.alignment.into()));
    obj.insert("fields".to_owned(), Value::Array(fields));
    obj.insert("kind".to_owned(), Value::String("record".to_owned()));
    obj.insert("name".to_owned(), Value::String(rec.name.clone()));
    obj.insert("size".to_owned(), Value::Number(layout.size.into()));
    Ok(Value::Object(obj))
}

fn enum_value(en: &EnumDef) -> Result<Value, CanonicalError> {
    let layout = primitive_layout(en.underlying);
    let mut values = Vec::new();
    for ev in &en.values {
        values.push(enum_value_value(ev));
    }

    let mut obj = Map::new();
    obj.insert("alignment".to_owned(), Value::Number(layout.alignment.into()));
    obj.insert("kind".to_owned(), Value::String("enum".to_owned()));
    obj.insert("name".to_owned(), Value::String(en.name.clone()));
    obj.insert("size".to_owned(), Value::Number(layout.size.into()));
    obj.insert("underlying".to_owned(), Value::String(primitive_name(en.underlying).to_owned()));
    obj.insert("values".to_owned(), Value::Array(values));
    Ok(Value::Object(obj))
}

fn enum_value_value(ev: &EnumValue) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(ev.name.clone()));
    obj.insert("value".to_owned(), Value::Number(ev.value.into()));
    Value::Object(obj)
}

fn field_layout_value(
    field: &Field,
    offset: usize,
    layout: Layout,
) -> Result<Value, CanonicalError> {
    let mut obj = Map::new();
    obj.insert("alignment".to_owned(), Value::Number(layout.alignment.into()));
    if let Some(max) = field.max {
        obj.insert("max".to_owned(), Value::Number(max.into()));
    }
    if let Some(min) = field.min {
        obj.insert("min".to_owned(), Value::Number(min.into()));
    }
    obj.insert("name".to_owned(), Value::String(field.name.clone()));
    obj.insert("offset".to_owned(), Value::Number(offset.into()));
    obj.insert("size".to_owned(), Value::Number(layout.size.into()));
    obj.insert("type".to_owned(), type_value(&field.ty));
    if let Some(unit) = &field.unit {
        obj.insert("unit".to_owned(), Value::String(unit.clone()));
    }
    Ok(Value::Object(obj))
}

fn field_layout_value_from(fl: FieldLayout) -> Result<Value, CanonicalError> {
    let mut obj = Map::new();
    obj.insert("alignment".to_owned(), Value::Number(fl.alignment.into()));
    obj.insert("name".to_owned(), Value::String(fl.name));
    obj.insert("offset".to_owned(), Value::Number(fl.offset.into()));
    obj.insert("size".to_owned(), Value::Number(fl.size.into()));
    obj.insert("type".to_owned(), type_value(&fl.ty));
    Ok(Value::Object(obj))
}

fn type_value(ty: &Type) -> Value {
    Value::String(type_string(ty))
}

fn type_string(ty: &Type) -> String {
    match ty {
        Type::Primitive(p) => primitive_name(*p).to_owned(),
        Type::Named(n) => n.clone(),
        Type::Array(element, count) => match count {
            ArrayCount::Literal(n) => format!("{}[{n}]", type_string(element)),
            ArrayCount::Param(name) => format!("{}[{name}]", type_string(element)),
        },
    }
}

fn version_value(v: Version) -> Value {
    let mut obj = Map::new();
    obj.insert("major".to_owned(), Value::Number(v.major.into()));
    obj.insert("minor".to_owned(), Value::Number(v.minor.into()));
    obj.insert("patch".to_owned(), Value::Number(v.patch.into()));
    Value::Object(obj)
}

fn build_registry(schema: &ContractSchema) -> HashMap<String, TypeDef> {
    let mut map = HashMap::new();
    for td in &schema.types {
        map.insert(td.name().to_owned(), td.clone());
    }
    map
}

pub(crate) fn primitive_layout(p: PrimitiveType) -> Layout {
    match p {
        PrimitiveType::I8 | PrimitiveType::U8 => Layout { size: 1, alignment: 1 },
        PrimitiveType::I16 | PrimitiveType::U16 => Layout { size: 2, alignment: 2 },
        PrimitiveType::I32 | PrimitiveType::U32 | PrimitiveType::F32 => Layout {
            size: 4,
            alignment: 4,
        },
        PrimitiveType::I64 | PrimitiveType::U64 | PrimitiveType::F64 => Layout {
            size: 8,
            alignment: 8,
        },
    }
}

fn primitive_name(p: PrimitiveType) -> &'static str {
    match p {
        PrimitiveType::I8 => "i8",
        PrimitiveType::I16 => "i16",
        PrimitiveType::I32 => "i32",
        PrimitiveType::I64 => "i64",
        PrimitiveType::U8 => "u8",
        PrimitiveType::U16 => "u16",
        PrimitiveType::U32 => "u32",
        PrimitiveType::U64 => "u64",
        PrimitiveType::F32 => "f32",
        PrimitiveType::F64 => "f64",
    }
}

fn type_layout(
    ty: &Type,
    registry: &HashMap<String, TypeDef>,
    cache: &mut HashMap<String, Layout>,
) -> Result<Layout, CanonicalError> {
    match ty {
        Type::Primitive(p) => Ok(primitive_layout(*p)),
        Type::Named(name) => {
            if let Some(layout) = cache.get(name) {
                return Ok(*layout);
            }
            let td = registry
                .get(name)
                .ok_or_else(|| CanonicalError::new(format!("unknown type '{name}'")))?;
            let layout = match td {
                TypeDef::Record(rec) => record_layout(&rec.fields, registry, cache)?.0,
                TypeDef::Enum(en) => primitive_layout(en.underlying),
            };
            cache.insert(name.clone(), layout);
            Ok(layout)
        }
        Type::Array(element, count) => {
            let el = type_layout(element, registry, cache)?;
            let count = match count {
                ArrayCount::Literal(n) => *n,
                ArrayCount::Param(name) => {
                    return Err(CanonicalError::new(format!(
                        "unresolved template parameter '{name}' in array size"
                    )));
                }
            };
            Ok(Layout {
                size: el.size.checked_mul(count).ok_or_else(|| {
                    CanonicalError::new(format!("array size overflow for type '{}'", type_string(ty)))
                })?,
                alignment: el.alignment,
            })
        }
    }
}

pub(crate) fn record_layout(
    fields: &[Field],
    registry: &HashMap<String, TypeDef>,
    cache: &mut HashMap<String, Layout>,
) -> Result<(Layout, Vec<FieldLayout>), CanonicalError> {
    let mut offset = 0usize;
    let mut alignment = 1usize;
    let mut layouts = Vec::with_capacity(fields.len());

    for field in fields {
        let layout = type_layout(&field.ty, registry, cache)?;
        alignment = alignment.max(layout.alignment);
        offset = align_to(offset, layout.alignment);
        layouts.push(FieldLayout {
            name: field.name.clone(),
            ty: field.ty.clone(),
            offset,
            size: layout.size,
            alignment: layout.alignment,
        });
        offset += layout.size;
    }

    let size = align_to(offset, alignment);
    Ok((Layout { size, alignment }, layouts))
}

fn align_to(offset: usize, alignment: usize) -> usize {
    if alignment == 0 {
        return offset;
    }
    (offset + alignment - 1) & !(alignment - 1)
}

/// Computes the layout of the top-level aggregate fields without hashing.
pub fn root_layout(schema: &ContractSchema) -> Result<(Layout, Vec<FieldLayout>), CanonicalError> {
    let registry = build_registry(schema);
    let mut cache = HashMap::new();
    record_layout(&schema.fields, &registry, &mut cache)
}

/// Format identifier for canonical component descriptors.
pub const COMPONENT_FORMAT: &str = "nxd-component-v1";

/// Emits the canonical UTF-8 JSON document and SHA-256 descriptor hash for a component.
pub fn emit_component(descriptor: &ComponentDescriptor) -> Result<(String, [u8; 32]), CanonicalError> {
    let value = component_to_value(descriptor)?;
    let json = json_to_string(&value)?;
    let hash = sha256(&json);
    Ok((json, hash))
}

fn component_to_value(descriptor: &ComponentDescriptor) -> Result<Value, CanonicalError> {
    let mut obj = Map::new();
    obj.insert("format".to_owned(), Value::String(COMPONENT_FORMAT.to_owned()));
    obj.insert("name".to_owned(), Value::String(descriptor.name.clone()));
    obj.insert(
        "version".to_owned(),
        version_value(descriptor.version),
    );

    let mut sdk = Map::new();
    sdk.insert(
        "abi_major".to_owned(),
        Value::Number(descriptor.sdk.abi_major.into()),
    );
    sdk.insert(
        "abi_minor".to_owned(),
        Value::Number(descriptor.sdk.abi_minor.into()),
    );
    obj.insert("sdk".to_owned(), Value::Object(sdk));

    let contracts: Vec<Value> = descriptor.contracts.iter().map(contract_ref_value).collect();
    obj.insert("contracts".to_owned(), Value::Array(contracts));

    let ports: Vec<Value> = descriptor.ports.iter().map(port_value).collect();
    obj.insert("ports".to_owned(), Value::Array(ports));

    let parameters: Vec<Value> = descriptor
        .parameters
        .iter()
        .map(parameter_value)
        .collect();
    obj.insert("parameters".to_owned(), Value::Array(parameters));

    let thread_roles: Vec<Value> = descriptor
        .thread_roles
        .iter()
        .map(thread_role_value)
        .collect();
    obj.insert("thread_roles".to_owned(), Value::Array(thread_roles));

    obj.insert(
        "resources".to_owned(),
        resources_value(&descriptor.resources),
    );

    let modes: Vec<Value> = descriptor
        .supported_modes
        .iter()
        .map(|m| Value::String(m.clone()))
        .collect();
    obj.insert("supported_modes".to_owned(), Value::Array(modes));

    Ok(Value::Object(obj))
}

fn contract_ref_value(contract: &ContractRef) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(contract.name.clone()));
    obj.insert("version".to_owned(), version_value(contract.version));
    if let Some(hash) = &contract.layout_hash {
        obj.insert("hash".to_owned(), Value::String(hash.clone()));
    }
    Value::Object(obj)
}

fn port_value(port: &Port) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(port.name.clone()));
    obj.insert(
        "direction".to_owned(),
        Value::String(port_direction_name(port.direction).to_owned()),
    );
    obj.insert(
        "semantics".to_owned(),
        Value::String(port_semantics_name(port.semantics).to_owned()),
    );
    obj.insert(
        "contract".to_owned(),
        Value::String(port.contract_name.clone()),
    );
    obj.insert(
        "contract_version".to_owned(),
        version_value(port.contract_version),
    );
    obj.insert(
        "required".to_owned(),
        Value::Bool(port.binding_required),
    );
    Value::Object(obj)
}

fn port_direction_name(direction: PortDirection) -> &'static str {
    match direction {
        PortDirection::In => "in",
        PortDirection::Out => "out",
        PortDirection::InOut => "inout",
    }
}

fn port_semantics_name(semantics: PortSemantics) -> &'static str {
    match semantics {
        PortSemantics::Signal => "signal",
        PortSemantics::Event => "event",
        PortSemantics::Stream => "stream",
        PortSemantics::Command => "command",
    }
}

fn parameter_value(parameter: &Parameter) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(parameter.name.clone()));
    obj.insert("type".to_owned(), type_value(&parameter.ty));
    if let Some(min) = parameter.min {
        obj.insert("min".to_owned(), Value::Number(min.into()));
    }
    if let Some(max) = parameter.max {
        obj.insert("max".to_owned(), Value::Number(max.into()));
    }
    obj.insert("file".to_owned(), Value::Bool(parameter.file));
    obj.insert("required".to_owned(), Value::Bool(parameter.required));
    Value::Object(obj)
}

fn thread_role_value(role: &ThreadRole) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(role.name.clone()));
    obj.insert("required".to_owned(), Value::Bool(role.required));
    obj.insert("single_cpu".to_owned(), Value::Bool(role.single_cpu));
    if let Some(schedule) = &role.default_schedule {
        obj.insert(
            "default_schedule".to_owned(),
            Value::String(schedule.clone()),
        );
    }
    Value::Object(obj)
}

fn resources_value(resources: &Resources) -> Value {
    let mut obj = Map::new();
    if let Some(max_heap) = resources.max_heap_bytes {
        obj.insert("max_heap_bytes".to_owned(), Value::Number(max_heap.into()));
    }
    obj.insert(
        "deterministic".to_owned(),
        Value::Bool(resources.deterministic),
    );
    obj.insert("uses_rng".to_owned(), Value::Bool(resources.uses_rng));
    obj.insert(
        "requires_hardware_rt".to_owned(),
        Value::Bool(resources.requires_hardware_rt),
    );
    Value::Object(obj)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::{Field, PrimitiveType, Type, Version};

    fn sample_schema() -> ContractSchema {
        ContractSchema {
            name: "Sample".to_owned(),
            version: Version::new(1, 0, 0),
            description: Some("ignored".to_owned()),
            params: Vec::new(),
            concrete: Vec::new(),
            types: Vec::new(),
            fields: vec![
                Field {
                    name: "a".to_owned(),
                    ty: Type::Primitive(PrimitiveType::U8),
                    description: Some("ignored".to_owned()),
                    unit: None,
                    min: None,
                    max: None,
                    default: Some("ignored".to_owned()),
                },
                Field {
                    name: "b".to_owned(),
                    ty: Type::Primitive(PrimitiveType::U64),
                    description: None,
                    unit: None,
                    min: None,
                    max: None,
                    default: None,
                },
            ],
        }
    }

    #[test]
    fn canonical_json_is_sorted() {
        let schema = sample_schema();
        let (json, _) = emit(&schema).unwrap();
        assert!(json.starts_with("{\"alignment\":"), "keys must be lexicographically sorted: {json}");
    }

    #[test]
    fn description_and_default_do_not_affect_hash() {
        let schema1 = sample_schema();
        let mut schema2 = sample_schema();
        schema2.fields[0].description = Some("changed".to_owned());
        schema2.fields[0].default = Some("changed".to_owned());
        schema2.description = Some("also changed".to_owned());

        let (_, hash1) = emit(&schema1).unwrap();
        let (_, hash2) = emit(&schema2).unwrap();
        assert_eq!(hash1, hash2);
    }

    #[test]
    fn layout_change_affects_hash() {
        let schema1 = sample_schema();
        let mut schema2 = sample_schema();
        schema2.fields[0].ty = Type::Primitive(PrimitiveType::U32);

        let (_, hash1) = emit(&schema1).unwrap();
        let (_, hash2) = emit(&schema2).unwrap();
        assert_ne!(hash1, hash2);
    }

    #[test]
    fn wire_semantic_change_affects_hash() {
        let mut schema1 = sample_schema();
        schema1.fields[0].unit = Some("m".to_owned());
        let mut schema2 = sample_schema();
        schema2.fields[0].unit = Some("mm".to_owned());

        let (_, hash1) = emit(&schema1).unwrap();
        let (_, hash2) = emit(&schema2).unwrap();
        assert_ne!(hash1, hash2);
    }

    #[test]
    fn repeated_runs_are_identical() {
        let schema = sample_schema();
        let (json1, hash1) = emit(&schema).unwrap();
        let (json2, hash2) = emit(&schema).unwrap();
        assert_eq!(json1, json2);
        assert_eq!(hash1, hash2);
    }

    #[test]
    fn root_layout_has_trailing_padding() {
        let schema = sample_schema();
        let (layout, fields) = root_layout(&schema).unwrap();
        // u8 a at 0, u64 b at 8, trailing padding to 8 -> size 16
        assert_eq!(layout.size, 16);
        assert_eq!(layout.alignment, 8);
        assert_eq!(fields[0].offset, 0);
        assert_eq!(fields[1].offset, 8);
    }
}
