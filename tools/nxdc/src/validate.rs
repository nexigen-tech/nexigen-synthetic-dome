//! Semantic validation of parsed contract schemas and component descriptors.
//!
//! Validation runs after parsing and performs cross-reference checks, rejects
//! unbounded wire types, and reports field-specific diagnostics.  It is
//! intentionally separate from parsing so that invalid type references and
//! unbounded declarations are reported with stable paths.

use crate::diag::Diagnostics;
use crate::model::{
    ArrayCount, ComponentDescriptor, ConcreteInstance, ContractSchema, Field, PrimitiveType,
    RecordDef, TemplateParam, TemplateParamKind, Type, TypeDef,
};
use crate::parse::parse_primitive;
use std::collections::{HashMap, HashSet};

/// Type names that are always unbounded or non-portable and therefore forbidden
/// on the wire, regardless of whether a user type with the same name exists.
const FORBIDDEN_TYPE_NAMES: &[&str] = &[
    "string", "String", "str", "Vec", "Box", "Rc", "Arc", "bool", "long", "size_t", "isize",
    "usize", "pointer", "Ptr", "Option",
];

/// Names that look like pointers or runtime indirection.
const POINTER_PATTERNS: &[&str] = &["*", "&", "->", "Ref<", "dyn "];

/// Validates a parsed contract schema template and returns the concrete
/// instantiations that should be emitted.  If the schema declares no template
/// parameters, a single concrete schema (the template itself) is returned.
pub fn validate_and_instantiate(
    schema: &ContractSchema,
) -> Result<Vec<ContractSchema>, Diagnostics> {
    let mut diag = Diagnostics::new();

    let param_map: HashMap<String, &TemplateParam> =
        schema.params.iter().map(|p| (p.name.clone(), p)).collect();

    let mut type_names = HashSet::new();
    for td in &schema.types {
        type_names.insert(td.name().to_owned());
    }

    // Validate template structure: type references and parameter usages.
    validate_fields_against_template(&schema.fields, "fields", &type_names, &param_map, &mut diag);
    for (idx, td) in schema.types.iter().enumerate() {
        let path = format!("types[{idx}]");
        match td {
            TypeDef::Record(rec) => {
                validate_fields_against_template(
                    &rec.fields,
                    &format!("{path}.fields"),
                    &type_names,
                    &param_map,
                    &mut diag,
                );
            }
            TypeDef::Enum(en) => {
                for (vidx, ev) in en.values.iter().enumerate() {
                    if !fits_in_type(ev.value, en.underlying) {
                        diag.push(
                            format!("{path}.values[{vidx}]"),
                            format!(
                                "value {} does not fit in underlying type {:?}",
                                ev.value, en.underlying
                            ),
                        );
                    }
                }
            }
        }
    }

    // Validate concrete instantiations.
    let mut concrete_names = HashSet::new();
    for (idx, instance) in schema.concrete.iter().enumerate() {
        let path = format!("concrete[{idx}]");
        if !concrete_names.insert(instance.name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate concrete instance name '{}'", instance.name),
            );
        }
        validate_concrete_instance(instance, &param_map, &path, &mut diag);
    }

    if !diag.is_empty() {
        return Err(diag);
    }

    if schema.params.is_empty() {
        // No template parameters: the schema is already concrete.
        return Ok(vec![schema.clone()]);
    }

    if schema.concrete.is_empty() {
        // Parameters exist but no instantiations: arrays would be unresolved.
        let mut d = Diagnostics::new();
        d.push(
            "concrete",
            "template parameters declared but no concrete instances provided",
        );
        return Err(d);
    }

    let mut result = Vec::with_capacity(schema.concrete.len());
    for instance in &schema.concrete {
        result.push(substitute_concrete(schema, instance));
    }

    // Validate each concrete schema independently.
    for (idx, concrete) in result.iter().enumerate() {
        if let Err(mut d) = validate_concrete_schema(concrete) {
            for err in &mut d.errors {
                err.path = format!("concrete[{idx}].{}", err.path);
            }
            diag.merge(d);
        }
    }

    if diag.is_empty() {
        Ok(result)
    } else {
        Err(diag)
    }
}

/// Validates a parsed component descriptor.
pub fn validate_component(descriptor: &ComponentDescriptor) -> Result<(), Diagnostics> {
    let mut diag = Diagnostics::new();

    if descriptor.sdk.abi_major == 0 {
        diag.push("sdk.abi_major", "SDK ABI major version must be non-zero");
    }

    let mut port_names = HashSet::new();
    for (idx, port) in descriptor.ports.iter().enumerate() {
        if port.contract_name.is_empty() {
            diag.push(
                format!("ports[{idx}].contract"),
                "port must reference a contract",
            );
        }
        if port.contract_version.major == 0 {
            diag.push(
                format!("ports[{idx}].contract_version"),
                "port contract major version must be non-zero",
            );
        }
        if !port_names.insert(&port.name) {
            diag.push(
                format!("ports[{idx}].name"),
                format!("duplicate port name '{}'", port.name),
            );
        }
    }

    let mut param_names = HashSet::new();
    for (idx, parameter) in descriptor.parameters.iter().enumerate() {
        validate_component_parameter_type(
            &parameter.ty,
            &format!("parameters[{idx}].type"),
            &mut diag,
        );
        if !param_names.insert(&parameter.name) {
            diag.push(
                format!("parameters[{idx}].name"),
                format!("duplicate parameter name '{}'", parameter.name),
            );
        }
    }

    let mut role_names = HashSet::new();
    for (idx, role) in descriptor.thread_roles.iter().enumerate() {
        if !role_names.insert(&role.name) {
            diag.push(
                format!("thread_roles[{idx}].name"),
                format!("duplicate thread role name '{}'", role.name),
            );
        }
    }

    for (idx, contract) in descriptor.contracts.iter().enumerate() {
        if contract.name.is_empty() {
            diag.push(
                format!("contracts[{idx}].name"),
                "contract reference must have a name",
            );
        }
        if contract.version.major == 0 {
            diag.push(
                format!("contracts[{idx}].version"),
                "contract major version must be non-zero",
            );
        }
    }

    if diag.is_empty() {
        Ok(())
    } else {
        Err(diag)
    }
}

fn validate_concrete_schema(schema: &ContractSchema) -> Result<(), Diagnostics> {
    let mut diag = Diagnostics::new();

    let mut type_names = HashSet::new();
    for td in &schema.types {
        type_names.insert(td.name().to_owned());
    }

    for (idx, field) in schema.fields.iter().enumerate() {
        validate_type(
            &field.ty,
            &type_names,
            &format!("fields[{idx}].type"),
            &mut diag,
        );
    }

    for (idx, td) in schema.types.iter().enumerate() {
        let path = format!("types[{idx}]");
        match td {
            TypeDef::Record(rec) => {
                for (fidx, field) in rec.fields.iter().enumerate() {
                    validate_type(
                        &field.ty,
                        &type_names,
                        &format!("{path}.fields[{fidx}].type"),
                        &mut diag,
                    );
                }
            }
            TypeDef::Enum(en) => {
                for (vidx, ev) in en.values.iter().enumerate() {
                    if !fits_in_type(ev.value, en.underlying) {
                        diag.push(
                            format!("{path}.values[{vidx}]"),
                            format!(
                                "value {} does not fit in underlying type {:?}",
                                ev.value, en.underlying
                            ),
                        );
                    }
                }
            }
        }
    }

    if diag.is_empty() {
        Ok(())
    } else {
        Err(diag)
    }
}

fn validate_fields_against_template(
    fields: &[Field],
    path: &str,
    type_names: &HashSet<String>,
    param_map: &HashMap<String, &TemplateParam>,
    diag: &mut Diagnostics,
) {
    for (idx, field) in fields.iter().enumerate() {
        validate_type_template(
            &field.ty,
            type_names,
            param_map,
            &format!("{path}[{idx}].type"),
            diag,
        );
    }
}

fn validate_type_template(
    ty: &Type,
    type_names: &HashSet<String>,
    param_map: &HashMap<String, &TemplateParam>,
    path: &str,
    diag: &mut Diagnostics,
) {
    match ty {
        Type::Primitive(_) => {}
        Type::Named(name) => {
            if param_map.contains_key(name) {
                // A named type that matches a template parameter is valid.
                if let Some(param) = param_map.get(name) {
                    if !matches!(param.kind, TemplateParamKind::TypeParam { .. }) {
                        diag.push(
                            path,
                            format!(
                                "template parameter '{name}' used as a type but declared as integer"
                            ),
                        );
                    }
                }
                return;
            }
            if FORBIDDEN_TYPE_NAMES.contains(&name.as_str()) {
                diag.push(
                    path,
                    format!("unbounded type '{name}' is not allowed on the wire"),
                );
            } else if name.contains('*')
                || name.contains('&')
                || POINTER_PATTERNS.iter().any(|p| name.contains(p))
            {
                diag.push(
                    path,
                    format!("pointer or reference type '{name}' is not allowed on the wire"),
                );
            } else if looks_like_generic(name) {
                diag.push(
                    path,
                    format!("unbounded generic type '{name}' is not allowed on the wire"),
                );
            } else if (looks_like_primitive(name) && parse_primitive(name).is_none())
                || !is_valid_identifier(name)
            {
                diag.push(path, format!("malformed type '{name}'"));
            } else if !type_names.contains(name) {
                diag.push(path, format!("unknown type reference '{name}'"));
            }
        }
        Type::Array(element, ArrayCount::Param(param)) => {
            if param_map.is_empty() {
                diag.push(path, format!("malformed array count '{param}'"));
            } else if let Some(template_param) = param_map.get(param) {
                if !matches!(template_param.kind, TemplateParamKind::Integer { .. }) {
                    diag.push(
                        path,
                        format!(
                            "array size parameter '{param}' must be an integer template parameter"
                        ),
                    );
                }
            } else {
                diag.push(
                    path,
                    format!("unknown template parameter '{param}' in array size"),
                );
            }
            validate_type_template(element, type_names, param_map, &format!("{path}[]"), diag);
        }
        Type::Array(element, ArrayCount::Literal(n)) => {
            if *n == 0 {
                diag.push(path, "array count must be positive");
            }
            validate_type_template(element, type_names, param_map, &format!("{path}[]"), diag);
        }
    }
}

fn validate_component_parameter_type(ty: &Type, path: &str, diag: &mut Diagnostics) {
    match ty {
        Type::Primitive(_) => {}
        Type::Named(name) => {
            if FORBIDDEN_TYPE_NAMES.contains(&name.as_str()) {
                diag.push(
                    path,
                    format!("unbounded type '{name}' is not allowed for a parameter"),
                );
            } else if name.contains('*')
                || name.contains('&')
                || POINTER_PATTERNS.iter().any(|p| name.contains(p))
            {
                diag.push(
                    path,
                    format!("pointer or reference type '{name}' is not allowed for a parameter"),
                );
            } else if looks_like_generic(name) {
                diag.push(
                    path,
                    format!("unbounded generic type '{name}' is not allowed for a parameter"),
                );
            } else if (looks_like_primitive(name) && parse_primitive(name).is_none())
                || !is_valid_identifier(name)
            {
                diag.push(path, format!("malformed type '{name}'"));
            }
            // Named user types are not resolved here because component parameters
            // are standalone scalar or array declarations, not contract types.
        }
        Type::Array(element, ArrayCount::Literal(n)) => {
            if *n == 0 {
                diag.push(path, "array count must be positive");
            }
            validate_component_parameter_type(element, &format!("{path}[]"), diag);
        }
        Type::Array(_, ArrayCount::Param(param)) => {
            diag.push(
                path,
                format!("unresolved template parameter '{param}' in parameter type"),
            );
        }
    }
}

fn validate_type(ty: &Type, type_names: &HashSet<String>, path: &str, diag: &mut Diagnostics) {
    match ty {
        Type::Primitive(_) => {}
        Type::Named(name) => {
            if FORBIDDEN_TYPE_NAMES.contains(&name.as_str()) {
                diag.push(
                    path,
                    format!("unbounded type '{name}' is not allowed on the wire"),
                );
            } else if name.contains('*')
                || name.contains('&')
                || POINTER_PATTERNS.iter().any(|p| name.contains(p))
            {
                diag.push(
                    path,
                    format!("pointer or reference type '{name}' is not allowed on the wire"),
                );
            } else if looks_like_generic(name) {
                diag.push(
                    path,
                    format!("unbounded generic type '{name}' is not allowed on the wire"),
                );
            } else if (looks_like_primitive(name) && parse_primitive(name).is_none())
                || !is_valid_identifier(name)
            {
                diag.push(path, format!("malformed type '{name}'"));
            } else if !type_names.contains(name) {
                diag.push(path, format!("unknown type reference '{name}'"));
            }
        }
        Type::Array(element, ArrayCount::Literal(n)) => {
            if *n == 0 {
                diag.push(path, "array count must be positive");
            }
            validate_type(element, type_names, &format!("{path}[]"), diag);
        }
        Type::Array(_, ArrayCount::Param(param)) => {
            diag.push(
                path,
                format!("unresolved template parameter '{param}' in concrete schema"),
            );
        }
    }
}

fn validate_concrete_instance(
    instance: &ConcreteInstance,
    param_map: &HashMap<String, &TemplateParam>,
    path: &str,
    diag: &mut Diagnostics,
) {
    let mut provided_integers = HashSet::new();
    let mut provided_types = HashSet::new();

    for (name, value) in &instance.values.integers {
        let param_path = format!("{path}.params.{name}");
        if !provided_integers.insert(name.clone()) {
            diag.push(
                &param_path,
                format!("duplicate parameter value for '{name}'"),
            );
            continue;
        }
        let Some(param) = param_map.get(name) else {
            diag.push(&param_path, format!("unknown template parameter '{name}'"));
            continue;
        };
        let TemplateParamKind::Integer { min, max, .. } = param.kind else {
            diag.push(
                &param_path,
                format!("parameter '{name}' is not an integer parameter"),
            );
            continue;
        };
        if let Some(min) = min {
            if *value < min {
                diag.push(&param_path, format!("value {value} is below minimum {min}"));
            }
        }
        if let Some(max) = max {
            if *value > max {
                diag.push(&param_path, format!("value {value} is above maximum {max}"));
            }
        }
    }

    for (name, value) in &instance.values.types {
        let param_path = format!("{path}.params.{name}");
        if !provided_types.insert(name.clone()) {
            diag.push(
                &param_path,
                format!("duplicate parameter value for '{name}'"),
            );
            continue;
        }
        let Some(param) = param_map.get(name) else {
            diag.push(&param_path, format!("unknown template parameter '{name}'"));
            continue;
        };
        let TemplateParamKind::TypeParam { allowed, .. } = &param.kind else {
            diag.push(
                &param_path,
                format!("parameter '{name}' is not a type parameter"),
            );
            continue;
        };
        if !allowed.contains(value) {
            let names: Vec<&'static str> = allowed.iter().map(|p| primitive_name(*p)).collect();
            diag.push(
                &param_path,
                format!(
                    "type '{}' is not in the allow-list ({})",
                    primitive_name(*value),
                    names.join(", ")
                ),
            );
        }
    }

    for param in param_map.values() {
        let has_value = match &param.kind {
            TemplateParamKind::Integer { default, .. } => {
                let provided = instance
                    .values
                    .integers
                    .iter()
                    .any(|(n, _)| n == &param.name);
                provided || default.is_some()
            }
            TemplateParamKind::TypeParam { default, .. } => {
                let provided = instance.values.types.iter().any(|(n, _)| n == &param.name);
                provided || default.is_some()
            }
        };
        if !has_value {
            diag.push(
                format!("{path}.params"),
                format!("missing required parameter '{}'", param.name),
            );
        }
    }
}

fn substitute_concrete(schema: &ContractSchema, instance: &ConcreteInstance) -> ContractSchema {
    let mut int_values: HashMap<String, i64> = HashMap::new();
    let mut type_values: HashMap<String, PrimitiveType> = HashMap::new();

    for param in &schema.params {
        match &param.kind {
            TemplateParamKind::Integer { default, .. } => {
                int_values.insert(param.name.clone(), default.unwrap_or(0));
            }
            TemplateParamKind::TypeParam { default, .. } => {
                if let Some(d) = default {
                    type_values.insert(param.name.clone(), *d);
                }
            }
        }
    }

    for (name, value) in &instance.values.integers {
        int_values.insert(name.clone(), *value);
    }
    for (name, value) in &instance.values.types {
        type_values.insert(name.clone(), *value);
    }

    ContractSchema {
        name: instance.name.clone(),
        version: schema.version,
        description: schema.description.clone(),
        params: Vec::new(),
        concrete: Vec::new(),
        types: schema
            .types
            .iter()
            .map(|td| substitute_type_def(td, &int_values, &type_values))
            .collect(),
        fields: schema
            .fields
            .iter()
            .map(|f| substitute_field(f, &int_values, &type_values))
            .collect(),
    }
}

fn substitute_type_def(
    td: &TypeDef,
    int_values: &HashMap<String, i64>,
    type_values: &HashMap<String, PrimitiveType>,
) -> TypeDef {
    match td {
        TypeDef::Record(rec) => TypeDef::Record(RecordDef {
            name: rec.name.clone(),
            fields: rec
                .fields
                .iter()
                .map(|f| substitute_field(f, int_values, type_values))
                .collect(),
        }),
        TypeDef::Enum(en) => TypeDef::Enum(en.clone()),
    }
}

fn substitute_field(
    field: &Field,
    int_values: &HashMap<String, i64>,
    type_values: &HashMap<String, PrimitiveType>,
) -> Field {
    Field {
        name: field.name.clone(),
        ty: substitute_type(&field.ty, int_values, type_values),
        description: field.description.clone(),
        unit: field.unit.clone(),
        min: field.min,
        max: field.max,
        default: field.default.clone(),
    }
}

fn substitute_type(
    ty: &Type,
    int_values: &HashMap<String, i64>,
    type_values: &HashMap<String, PrimitiveType>,
) -> Type {
    match ty {
        Type::Primitive(p) => Type::Primitive(*p),
        Type::Named(n) => {
            if let Some(primitive) = type_values.get(n) {
                Type::Primitive(*primitive)
            } else {
                Type::Named(n.clone())
            }
        }
        Type::Array(element, count) => {
            let new_count = match count {
                ArrayCount::Literal(n) => ArrayCount::Literal(*n),
                ArrayCount::Param(name) => {
                    let n = int_values.get(name).copied().unwrap_or(0) as usize;
                    ArrayCount::Literal(n)
                }
            };
            Type::Array(
                Box::new(substitute_type(element, int_values, type_values)),
                new_count,
            )
        }
    }
}

fn looks_like_primitive(s: &str) -> bool {
    s.len() >= 2
        && matches!(s.as_bytes()[0], b'i' | b'u' | b'f')
        && s[1..].chars().all(|c| c.is_ascii_digit())
}

fn looks_like_generic(s: &str) -> bool {
    s.contains('<') || s.contains('>') || s.contains('(') || s.contains(')')
}

fn is_valid_identifier(s: &str) -> bool {
    let mut chars = s.chars();
    let Some(first) = chars.next() else {
        return false;
    };
    if !(first.is_ascii_alphabetic() || first == '_') {
        return false;
    }
    chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
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

fn fits_in_type(value: u64, underlying: PrimitiveType) -> bool {
    match underlying {
        PrimitiveType::U8 => value <= u8::MAX as u64,
        PrimitiveType::U16 => value <= u16::MAX as u64,
        PrimitiveType::U32 => value <= u32::MAX as u64,
        PrimitiveType::U64 => true,
        PrimitiveType::I8 => value <= i8::MAX as u64,
        PrimitiveType::I16 => value <= i16::MAX as u64,
        PrimitiveType::I32 => value <= i32::MAX as u64,
        PrimitiveType::I64 => true,
        PrimitiveType::F32 | PrimitiveType::F64 => false,
    }
}
