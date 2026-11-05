//! Cold-path field descriptor emission as JSON.

use crate::canonical::{self, FieldLayout};
use crate::diag::Diagnostics;
use crate::model::{ArrayCount, ContractSchema, EnumDef, EnumValue, PrimitiveType, Type, TypeDef};
use serde_json::{Map, Value};
use std::collections::HashMap;

/// Emits a machine-readable field descriptor JSON document for a concrete schema.
pub fn emit(schema: &ContractSchema) -> Result<String, Diagnostics> {
    let registry = build_registry(schema);
    let mut root_fields = Vec::new();
    let (_, field_layouts) = canonical::root_layout(schema).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push(schema.name.clone(), e.to_string());
        d
    })?;
    for fl in field_layouts {
        root_fields.push(field_descriptor(&fl, &registry)?);
    }

    let mut types = Vec::new();
    for td in &schema.types {
        types.push(type_descriptor(td, &registry)?);
    }

    let mut obj = Map::new();
    obj.insert("contract".to_owned(), Value::String(schema.name.clone()));
    obj.insert("version".to_owned(), version_value(schema.version));
    obj.insert("fields".to_owned(), Value::Array(root_fields));
    obj.insert("types".to_owned(), Value::Array(types));

    serde_json::to_string_pretty(&Value::Object(obj)).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push("descriptor", format!("JSON serialization failed: {e}"));
        d
    })
}

fn build_registry(schema: &ContractSchema) -> HashMap<String, TypeDef> {
    let mut map = HashMap::new();
    for td in &schema.types {
        map.insert(td.name().to_owned(), td.clone());
    }
    map
}

fn type_descriptor(
    td: &TypeDef,
    registry: &HashMap<String, TypeDef>,
) -> Result<Value, Diagnostics> {
    match td {
        TypeDef::Record(rec) => {
            let mut obj = Map::new();
            obj.insert("kind".to_owned(), Value::String("record".to_owned()));
            obj.insert("name".to_owned(), Value::String(rec.name.clone()));
            let mut fields = Vec::new();
            let (_, layouts) = canonical::record_layout(&rec.fields, registry, &mut HashMap::new())
                .map_err(|e| {
                    let mut d = Diagnostics::new();
                    d.push(format!("types.{}", rec.name), e.to_string());
                    d
                })?;
            for fl in layouts {
                fields.push(field_descriptor(&fl, registry)?);
            }
            obj.insert("fields".to_owned(), Value::Array(fields));
            Ok(Value::Object(obj))
        }
        TypeDef::Enum(en) => enum_descriptor(en),
    }
}

fn enum_descriptor(en: &EnumDef) -> Result<Value, Diagnostics> {
    let mut obj = Map::new();
    obj.insert("kind".to_owned(), Value::String("enum".to_owned()));
    obj.insert("name".to_owned(), Value::String(en.name.clone()));
    obj.insert(
        "underlying".to_owned(),
        Value::String(primitive_name(en.underlying).to_owned()),
    );
    let mut values = Vec::new();
    for ev in &en.values {
        values.push(enum_value_descriptor(ev));
    }
    obj.insert("values".to_owned(), Value::Array(values));
    Ok(Value::Object(obj))
}

fn enum_value_descriptor(ev: &EnumValue) -> Value {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(ev.name.clone()));
    obj.insert("value".to_owned(), Value::Number(ev.value.into()));
    Value::Object(obj)
}

fn field_descriptor(
    fl: &FieldLayout,
    registry: &HashMap<String, TypeDef>,
) -> Result<Value, Diagnostics> {
    let mut obj = Map::new();
    obj.insert("name".to_owned(), Value::String(fl.name.clone()));
    obj.insert("type".to_owned(), type_string_value(&fl.ty));
    obj.insert("offset".to_owned(), Value::Number(fl.offset.into()));
    obj.insert("size".to_owned(), Value::Number(fl.size.into()));
    obj.insert("alignment".to_owned(), Value::Number(fl.alignment.into()));
    if let Some(labels) = enum_labels(&fl.ty, registry) {
        obj.insert("enum_labels".to_owned(), Value::Array(labels));
    }
    Ok(Value::Object(obj))
}

fn enum_labels(ty: &Type, registry: &HashMap<String, TypeDef>) -> Option<Vec<Value>> {
    let name = match ty {
        Type::Named(n) => n,
        _ => return None,
    };
    let td = registry.get(name)?;
    let en = match td {
        TypeDef::Enum(en) => en,
        _ => return None,
    };
    Some(
        en.values
            .iter()
            .map(|ev| {
                let mut obj = Map::new();
                obj.insert("name".to_owned(), Value::String(ev.name.clone()));
                obj.insert("value".to_owned(), Value::Number(ev.value.into()));
                Value::Object(obj)
            })
            .collect(),
    )
}

fn type_string_value(ty: &Type) -> Value {
    Value::String(match ty {
        Type::Primitive(p) => primitive_name(*p).to_owned(),
        Type::Named(n) => n.clone(),
        Type::Array(element, ArrayCount::Literal(n)) => {
            format!("{}[{n}]", type_string(element))
        }
        Type::Array(element, ArrayCount::Param(name)) => {
            format!("{}[{name}]", type_string(element))
        }
    })
}

fn type_string(ty: &Type) -> String {
    match ty {
        Type::Primitive(p) => primitive_name(*p).to_owned(),
        Type::Named(n) => n.clone(),
        Type::Array(element, ArrayCount::Literal(n)) => format!("{}[{n}]", type_string(element)),
        Type::Array(element, ArrayCount::Param(name)) => {
            format!("{}[{name}]", type_string(element))
        }
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

fn version_value(v: crate::model::Version) -> Value {
    let mut obj = Map::new();
    obj.insert("major".to_owned(), Value::Number(v.major.into()));
    obj.insert("minor".to_owned(), Value::Number(v.minor.into()));
    obj.insert("patch".to_owned(), Value::Number(v.patch.into()));
    Value::Object(obj)
}
