//! YAML parsing for contract schemas and component descriptors.
//!
//! Parsing is deliberately defensive: every node is inspected and converted
//! into the validated model with a path, and malformed or unexpected nodes
//! produce field-specific diagnostics rather than panics.

use crate::diag::Diagnostics;
use crate::model::{
    ArrayCount, ComponentDescriptor, ConcreteInstance, ConcreteValues, ContractRef, ContractSchema,
    EnumDef, EnumValue, Field, Parameter, Port, PortDirection, PortSemantics, PrimitiveType,
    RecordDef, Resources, SdkVersion, TemplateParam, TemplateParamKind, ThreadRole, Type, TypeDef,
    Version,
};
use std::path::Path;
use yaml_rust2::{Yaml, YamlLoader};

/// Loads a contract schema from a YAML file.
pub fn load_contract(path: &Path, text: &str) -> Result<ContractSchema, Diagnostics> {
    let docs = YamlLoader::load_from_str(text).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push("<yaml>", format!("YAML parse error: {e}"));
        d
    })?;

    if docs.is_empty() {
        let mut d = Diagnostics::new();
        d.push("<yaml>", "empty YAML document");
        return Err(d);
    }

    let root = &docs[0];
    parse_contract_document(path, root)
}

/// Loads a component descriptor from a YAML file.
pub fn load_component(path: &Path, text: &str) -> Result<ComponentDescriptor, Diagnostics> {
    let docs = YamlLoader::load_from_str(text).map_err(|e| {
        let mut d = Diagnostics::new();
        d.push("<yaml>", format!("YAML parse error: {e}"));
        d
    })?;

    if docs.is_empty() {
        let mut d = Diagnostics::new();
        d.push("<yaml>", "empty YAML document");
        return Err(d);
    }

    let root = &docs[0];
    parse_component_document(path, root)
}

fn parse_contract_document(_path: &Path, root: &Yaml) -> Result<ContractSchema, Diagnostics> {
    let mut diag = Diagnostics::new();
    let contract_node = require_hash(root, "contract", &mut diag);

    let (name, version, description) = if let Some(node) = contract_node {
        parse_contract_header(node, &mut diag)
    } else {
        (None, None, None)
    };

    let params = lookup(root, "params")
        .map(|n| parse_template_params(n, &mut diag))
        .unwrap_or_default();

    let concrete = lookup(root, "concrete")
        .map(|n| parse_concrete_instances(n, &mut diag))
        .unwrap_or_default();

    let types = lookup(root, "types")
        .map(|n| parse_types(n, &mut diag))
        .unwrap_or_default();

    let fields = lookup(root, "fields")
        .map(|n| parse_fields(n, "fields", &mut diag))
        .unwrap_or_default();

    if diag.is_empty() {
        Ok(ContractSchema {
            name: name.unwrap_or_default(),
            version: version.unwrap_or(Version::new(0, 0, 0)),
            description,
            params,
            concrete,
            types,
            fields,
        })
    } else {
        Err(diag)
    }
}

fn parse_contract_header(
    node: &Yaml,
    diag: &mut Diagnostics,
) -> (Option<String>, Option<Version>, Option<String>) {
    let name = get_string(node, "contract.name", diag);
    let version = lookup(node, "version").and_then(|v| parse_version(v, "contract.version", diag));
    let description = lookup(node, "description").and_then(|v| {
        if let Some(s) = v.as_str() {
            Some(s.to_owned())
        } else {
            diag.push("contract.description", "expected string");
            None
        }
    });
    (name, version, description)
}

fn parse_template_params(node: &Yaml, diag: &mut Diagnostics) -> Vec<TemplateParam> {
    let Some(items) = node.as_vec() else {
        diag.push("params", "expected array of template parameter definitions");
        return Vec::new();
    };

    let mut params = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("params[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected template parameter mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate template parameter name '{name}'"),
            );
            continue;
        }

        // Optional "kind" field selects between integer parameter (default) and
        // type parameter.
        let kind = lookup_in_hash(item_hash, "kind")
            .and_then(|v| v.as_str())
            .unwrap_or("integer");

        match kind {
            "integer" => {
                let Some(type_str) =
                    hash_get_string(item_hash, "type", &format!("{path}.type"), diag)
                else {
                    continue;
                };
                let Some(ty) = parse_primitive(&type_str) else {
                    diag.push(
                        format!("{path}.type"),
                        format!(
                            "template parameter type '{type_str}' must be an integer primitive"
                        ),
                    );
                    continue;
                };
                if !ty.is_integer() {
                    diag.push(
                        format!("{path}.type"),
                        format!(
                            "template parameter type '{type_str}' must be an integer primitive"
                        ),
                    );
                    continue;
                }
                let min = lookup_in_hash(item_hash, "min").and_then(|v| v.as_i64());
                let max = lookup_in_hash(item_hash, "max").and_then(|v| v.as_i64());
                let default = lookup_in_hash(item_hash, "default").and_then(|v| v.as_i64());
                params.push(TemplateParam {
                    name,
                    kind: TemplateParamKind::Integer {
                        ty,
                        min,
                        max,
                        default,
                    },
                });
            }
            "type" => {
                let allowed = match lookup_in_hash(item_hash, "allowed").and_then(|v| v.as_vec()) {
                    Some(items) => {
                        let mut allowed = Vec::new();
                        for (aidx, aitem) in items.iter().enumerate() {
                            let apath = format!("{path}.allowed[{aidx}]");
                            let Some(name) = aitem.as_str() else {
                                diag.push(&apath, "expected primitive type name");
                                continue;
                            };
                            let Some(ty) = parse_primitive(name) else {
                                diag.push(
                                    &apath,
                                    format!("'{name}' is not a valid primitive type"),
                                );
                                continue;
                            };
                            allowed.push(ty);
                        }
                        if allowed.len() != items.len() {
                            // Errors already emitted for failed entries.
                            continue;
                        }
                        allowed
                    }
                    None => {
                        diag.push(
                            format!("{path}.allowed"),
                            "expected array of allowed primitive types",
                        );
                        continue;
                    }
                };
                let default = lookup_in_hash(item_hash, "default")
                    .and_then(|v| v.as_str())
                    .and_then(parse_primitive);
                params.push(TemplateParam {
                    name,
                    kind: TemplateParamKind::TypeParam { allowed, default },
                });
            }
            _ => {
                diag.push(
                    format!("{path}.kind"),
                    format!("unknown template parameter kind '{kind}'"),
                );
            }
        }
    }

    params
}

fn parse_concrete_instances(node: &Yaml, diag: &mut Diagnostics) -> Vec<ConcreteInstance> {
    let Some(items) = node.as_vec() else {
        diag.push("concrete", "expected array of concrete instantiations");
        return Vec::new();
    };

    let mut instances = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("concrete[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected concrete instantiation mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate concrete instance name '{name}'"),
            );
            continue;
        }

        let mut values = ConcreteValues::default();
        match lookup_in_hash(item_hash, "params").and_then(|v| v.as_hash()) {
            Some(p) => {
                for (k, v) in p.iter() {
                    let key = k.as_str().unwrap_or("").to_owned();
                    if key.is_empty() {
                        diag.push(&path, "concrete parameter name must be a string");
                        continue;
                    }
                    if let Some(number) = v.as_i64() {
                        values.integers.push((key, number));
                    } else if let Some(type_name) = v.as_str() {
                        if let Some(ty) = parse_primitive(type_name) {
                            values.types.push((key, ty));
                        } else {
                            diag.push(
                                format!("{path}.params.{key}"),
                                format!("'{type_name}' is not a valid concrete type value"),
                            );
                        }
                    } else {
                        diag.push(
                            format!("{path}.params.{key}"),
                            "expected integer or primitive type value",
                        );
                    }
                }
            }
            None => {
                diag.push(format!("{path}.params"), "expected parameter mapping");
            }
        };
        instances.push(ConcreteInstance { name, values });
    }

    instances
}

fn parse_types(node: &Yaml, diag: &mut Diagnostics) -> Vec<TypeDef> {
    let Some(items) = node.as_vec() else {
        diag.push("types", "expected array of type definitions");
        return Vec::new();
    };

    let mut types = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("types[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected type definition mapping");
            continue;
        };

        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };

        if !is_valid_identifier(&name) {
            diag.push(
                format!("{path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }

        if !seen.insert(name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate type name '{name}'"),
            );
            continue;
        }

        let Some(kind) = hash_get_string(item_hash, "kind", &format!("{path}.kind"), diag) else {
            continue;
        };

        match kind.as_str() {
            "record" => {
                let fields = lookup_in_hash(item_hash, "fields")
                    .map(|n| parse_fields(n, &format!("{path}.fields"), diag))
                    .unwrap_or_default();
                types.push(TypeDef::Record(RecordDef { name, fields }));
            }
            "enum" => {
                let Some(underlying_name) =
                    hash_get_string(item_hash, "underlying", &format!("{path}.underlying"), diag)
                else {
                    continue;
                };
                let Some(underlying) = parse_primitive(&underlying_name) else {
                    diag.push(
                        format!("{path}.underlying"),
                        format!("'{underlying_name}' is not a valid integer underlying type"),
                    );
                    continue;
                };
                if !underlying.is_integer() {
                    diag.push(
                        format!("{path}.underlying"),
                        format!("enum underlying type '{underlying_name}' must be an integer"),
                    );
                    continue;
                }
                let values = parse_enum_values(
                    lookup_in_hash(item_hash, "values"),
                    &format!("{path}.values"),
                    diag,
                );
                types.push(TypeDef::Enum(EnumDef {
                    name,
                    underlying,
                    values,
                }));
            }
            _ => {
                diag.push(
                    format!("{path}.kind"),
                    format!("unknown type kind '{kind}'"),
                );
            }
        }
    }

    types
}

fn parse_enum_values(node: Option<&Yaml>, path: &str, diag: &mut Diagnostics) -> Vec<EnumValue> {
    let Some(node) = node else {
        diag.push(path, "expected enum values array");
        return Vec::new();
    };
    let Some(items) = node.as_vec() else {
        diag.push(path, "expected array of enum values");
        return Vec::new();
    };

    let mut values = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let item_path = format!("{path}[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&item_path, "expected enum value mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{item_path}.name"), diag)
        else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{item_path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{item_path}.name"),
                format!("duplicate enum value name '{name}'"),
            );
            continue;
        }
        let value = if let Some(v) = lookup_in_hash(item_hash, "value").and_then(|v| v.as_i64()) {
            v as u64
        } else {
            diag.push(
                format!("{item_path}.value"),
                "expected non-negative integer enum value",
            );
            continue;
        };
        values.push(EnumValue { name, value });
    }

    values
}

fn parse_fields(node: &Yaml, path: &str, diag: &mut Diagnostics) -> Vec<Field> {
    let Some(items) = node.as_vec() else {
        diag.push(path, "expected array of field definitions");
        return Vec::new();
    };

    let mut fields = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let item_path = format!("{path}[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&item_path, "expected field definition mapping");
            continue;
        };

        let Some(name) = hash_get_string(item_hash, "name", &format!("{item_path}.name"), diag)
        else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{item_path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{item_path}.name"),
                format!("duplicate field name '{name}'"),
            );
            continue;
        }

        let Some(type_str) = hash_get_string(item_hash, "type", &format!("{item_path}.type"), diag)
        else {
            continue;
        };
        let ty = match parse_type_str(&type_str) {
            Ok(t) => t,
            Err(e) => {
                diag.push(format!("{item_path}.type"), e);
                continue;
            }
        };

        let description = lookup_in_hash(item_hash, "description").and_then(|v| {
            if let Some(s) = v.as_str() {
                Some(s.to_owned())
            } else {
                diag.push(format!("{item_path}.description"), "expected string");
                None
            }
        });
        let unit = lookup_in_hash(item_hash, "unit").and_then(|v| {
            if let Some(s) = v.as_str() {
                Some(s.to_owned())
            } else {
                diag.push(format!("{item_path}.unit"), "expected string");
                None
            }
        });
        let min = lookup_in_hash(item_hash, "min").and_then(|v| v.as_i64());
        let max = lookup_in_hash(item_hash, "max").and_then(|v| v.as_i64());
        let default = lookup_in_hash(item_hash, "default").map(|v| {
            v.as_str()
                .map(|s| s.to_owned())
                .or_else(|| v.as_i64().map(|n| n.to_string()))
                .unwrap_or_else(|| format!("{:?}", v).trim_matches('"').to_owned())
        });

        fields.push(Field {
            name,
            ty,
            description,
            unit,
            min,
            max,
            default,
        });
    }

    fields
}

fn parse_type_str(s: &str) -> Result<Type, String> {
    // Array syntax: T[N]
    if let Some(open) = s.rfind('[') {
        let close = s.rfind(']').filter(|&c| c > open);
        if close.is_none() {
            return Err(format!("malformed type '{s}': missing closing ']'"));
        }
        let close = close.unwrap();
        let inner = &s[..open];
        let count_str = &s[open + 1..close];
        if count_str.is_empty() {
            return Err(format!("malformed array count in type '{s}'"));
        }
        let count = if let Ok(n) = count_str.parse::<usize>() {
            if n == 0 {
                return Err(format!("array count must be positive in type '{s}'"));
            }
            ArrayCount::Literal(n)
        } else if is_valid_identifier(count_str) {
            ArrayCount::Param(count_str.to_owned())
        } else {
            return Err(format!("malformed array count '{count_str}' in type '{s}'"));
        };
        let element = parse_type_str(inner)?;
        return Ok(Type::Array(Box::new(element), count));
    }

    if let Some(p) = parse_primitive(s) {
        return Ok(Type::Primitive(p));
    }

    // Any other non-array string is forwarded as a named type reference.
    // Semantic validation decides whether it is a valid user type, a known
    // template parameter, a known unbounded type (e.g. Vec<u8>), or a malformed
    // primitive (e.g. f128).
    Ok(Type::Named(s.to_owned()))
}

fn parse_version(node: &Yaml, path: &str, diag: &mut Diagnostics) -> Option<Version> {
    let s = node.as_str().unwrap_or("");
    match parse_version_str(s) {
        Some(v) => Some(v),
        None => {
            diag.push(path, format!("invalid version '{s}'"));
            None
        }
    }
}

pub fn parse_version_str(s: &str) -> Option<Version> {
    let parts: Vec<&str> = s.split('.').collect();
    if parts.len() != 3 {
        return None;
    }
    let mut nums = [0u64; 3];
    for (i, part) in parts.iter().enumerate() {
        nums[i] = part.parse().ok()?;
    }
    Some(Version::new(nums[0], nums[1], nums[2]))
}

fn parse_component_document(_path: &Path, root: &Yaml) -> Result<ComponentDescriptor, Diagnostics> {
    let mut diag = Diagnostics::new();
    let node = require_hash(root, "component", &mut diag);
    let (name, version, _description) = if let Some(node) = node {
        parse_component_header(node, &mut diag)
    } else {
        (None, None, None)
    };

    let sdk = lookup(root, "sdk")
        .map(|n| parse_sdk(n, &mut diag))
        .unwrap_or(SdkVersion {
            abi_major: 0,
            abi_minor: 0,
        });

    let contracts = lookup(root, "contracts")
        .map(|n| parse_contract_refs(n, &mut diag))
        .unwrap_or_default();

    let ports = lookup(root, "ports")
        .map(|n| parse_ports(n, &mut diag))
        .unwrap_or_default();

    let parameters = lookup(root, "parameters")
        .map(|n| parse_parameters(n, &mut diag))
        .unwrap_or_default();

    let thread_roles = lookup(root, "thread_roles")
        .map(|n| parse_thread_roles(n, &mut diag))
        .unwrap_or_default();

    let resources = lookup(root, "resources")
        .map(|n| parse_resources(n, &mut diag))
        .unwrap_or_default();

    let supported_modes = lookup(root, "supported_modes")
        .and_then(|n| n.as_vec())
        .map(|v| {
            v.iter()
                .filter_map(|x| x.as_str().map(|s| s.to_owned()))
                .collect()
        })
        .unwrap_or_default();

    if diag.is_empty() {
        Ok(ComponentDescriptor {
            name: name.unwrap_or_default(),
            version: version.unwrap_or(Version::new(0, 0, 0)),
            sdk,
            contracts,
            ports,
            parameters,
            thread_roles,
            resources,
            supported_modes,
        })
    } else {
        Err(diag)
    }
}

fn parse_component_header(
    node: &Yaml,
    diag: &mut Diagnostics,
) -> (Option<String>, Option<Version>, Option<String>) {
    let name = get_string(node, "component.name", diag);
    let version = lookup(node, "version").and_then(|v| parse_version(v, "component.version", diag));
    let description = lookup(node, "description").and_then(|v| {
        if let Some(s) = v.as_str() {
            Some(s.to_owned())
        } else {
            diag.push("component.description", "expected string");
            None
        }
    });
    (name, version, description)
}

fn parse_sdk(node: &Yaml, diag: &mut Diagnostics) -> SdkVersion {
    let Some(hash) = node.as_hash() else {
        diag.push("sdk", "expected mapping");
        return SdkVersion {
            abi_major: 0,
            abi_minor: 0,
        };
    };
    let major = lookup_in_hash(hash, "abi_major")
        .and_then(|v| v.as_i64())
        .map(|v| v as u64)
        .unwrap_or(0);
    let minor = lookup_in_hash(hash, "abi_minor")
        .and_then(|v| v.as_i64())
        .map(|v| v as u64)
        .unwrap_or(0);
    SdkVersion {
        abi_major: major,
        abi_minor: minor,
    }
}

fn parse_contract_refs(node: &Yaml, diag: &mut Diagnostics) -> Vec<ContractRef> {
    let Some(items) = node.as_vec() else {
        diag.push("contracts", "expected array of contract references");
        return Vec::new();
    };

    let mut refs = Vec::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("contracts[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected contract reference mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        let version = lookup_in_hash(item_hash, "version")
            .and_then(|v| parse_version(v, &format!("{path}.version"), diag))
            .unwrap_or(Version::new(0, 0, 0));
        let layout_hash = hash_get_string(item_hash, "hash", &format!("{path}.hash"), diag);
        refs.push(ContractRef {
            name,
            version,
            layout_hash,
        });
    }

    refs
}

fn parse_parameters(node: &Yaml, diag: &mut Diagnostics) -> Vec<Parameter> {
    let Some(items) = node.as_vec() else {
        diag.push("parameters", "expected array of parameter definitions");
        return Vec::new();
    };

    let mut params = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("parameters[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected parameter mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate parameter name '{name}'"),
            );
            continue;
        }
        let Some(type_str) = hash_get_string(item_hash, "type", &format!("{path}.type"), diag)
        else {
            continue;
        };
        let ty = match parse_type_str(&type_str) {
            Ok(t) => t,
            Err(e) => {
                diag.push(format!("{path}.type"), e);
                continue;
            }
        };
        let description = lookup_in_hash(item_hash, "description").and_then(|v| {
            if let Some(s) = v.as_str() {
                Some(s.to_owned())
            } else {
                diag.push(format!("{path}.description"), "expected string");
                None
            }
        });
        let default = lookup_in_hash(item_hash, "default").map(|v| {
            v.as_str()
                .map(|s| s.to_owned())
                .or_else(|| v.as_i64().map(|n| n.to_string()))
                .unwrap_or_else(|| format!("{:?}", v).trim_matches('"').to_owned())
        });
        let min = lookup_in_hash(item_hash, "min").and_then(|v| v.as_i64());
        let max = lookup_in_hash(item_hash, "max").and_then(|v| v.as_i64());
        let file = lookup_in_hash(item_hash, "file")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        let required = lookup_in_hash(item_hash, "required")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        params.push(Parameter {
            name,
            ty,
            description,
            default,
            min,
            max,
            file,
            required,
        });
    }

    params
}

fn parse_ports(node: &Yaml, diag: &mut Diagnostics) -> Vec<Port> {
    let Some(items) = node.as_vec() else {
        diag.push("ports", "expected array of port definitions");
        return Vec::new();
    };

    let mut ports = Vec::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("ports[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected port mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        let direction =
            match hash_get_string(item_hash, "direction", &format!("{path}.direction"), diag)
                .as_deref()
            {
                Some("in") => PortDirection::In,
                Some("out") => PortDirection::Out,
                Some("inout") => PortDirection::InOut,
                Some(d) => {
                    diag.push(
                        format!("{path}.direction"),
                        format!("unknown direction '{d}'"),
                    );
                    continue;
                }
                None => continue,
            };
        let Some(contract_name) =
            hash_get_string(item_hash, "contract", &format!("{path}.contract"), diag)
        else {
            continue;
        };
        let contract_version = lookup_in_hash(item_hash, "contract_version")
            .and_then(|v| parse_version(v, &format!("{path}.contract_version"), diag))
            .unwrap_or(Version::new(0, 0, 0));
        let semantics =
            match hash_get_string(item_hash, "semantics", &format!("{path}.semantics"), diag)
                .as_deref()
            {
                Some("signal") => PortSemantics::Signal,
                Some("event") => PortSemantics::Event,
                Some("stream") => PortSemantics::Stream,
                Some("command") => PortSemantics::Command,
                Some(s) => {
                    diag.push(
                        format!("{path}.semantics"),
                        format!("unknown semantics '{s}'"),
                    );
                    continue;
                }
                None => continue,
            };
        let binding_required = lookup_in_hash(item_hash, "required")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        let layout_hash = hash_get_string(item_hash, "layout_hash", &format!("{path}.layout_hash"), diag);
        ports.push(Port {
            name,
            direction,
            semantics,
            contract_name,
            contract_version,
            layout_hash,
            binding_required,
        });
    }

    ports
}

fn parse_thread_roles(node: &Yaml, diag: &mut Diagnostics) -> Vec<ThreadRole> {
    let Some(items) = node.as_vec() else {
        diag.push("thread_roles", "expected array of thread role definitions");
        return Vec::new();
    };

    let mut roles = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for (idx, item) in items.iter().enumerate() {
        let path = format!("thread_roles[{idx}]");
        let Some(item_hash) = item.as_hash() else {
            diag.push(&path, "expected thread role mapping");
            continue;
        };
        let Some(name) = hash_get_string(item_hash, "name", &format!("{path}.name"), diag) else {
            continue;
        };
        if !is_valid_identifier(&name) {
            diag.push(
                format!("{path}.name"),
                format!("'{name}' is not a valid identifier"),
            );
            continue;
        }
        if !seen.insert(name.clone()) {
            diag.push(
                format!("{path}.name"),
                format!("duplicate thread role name '{name}'"),
            );
            continue;
        }
        let required = lookup_in_hash(item_hash, "required")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        let single_cpu = lookup_in_hash(item_hash, "single_cpu")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        let default_schedule = lookup_in_hash(item_hash, "default_schedule")
            .and_then(|v| v.as_str())
            .map(|s| s.to_owned());
        roles.push(ThreadRole {
            name,
            required,
            single_cpu,
            default_schedule,
        });
    }

    roles
}

fn parse_resources(node: &Yaml, diag: &mut Diagnostics) -> Resources {
    let mut res = Resources::default();
    let Some(hash) = node.as_hash() else {
        diag.push("resources", "expected mapping");
        return res;
    };
    res.max_heap_bytes = lookup_in_hash(hash, "max_heap_bytes")
        .and_then(|v| v.as_i64())
        .map(|v| v as u64);
    res.deterministic = lookup_in_hash(hash, "deterministic")
        .and_then(|v| v.as_bool())
        .unwrap_or(res.deterministic);
    res.uses_rng = lookup_in_hash(hash, "uses_rng")
        .and_then(|v| v.as_bool())
        .unwrap_or(res.uses_rng);
    res.requires_hardware_rt = lookup_in_hash(hash, "requires_hardware_rt")
        .and_then(|v| v.as_bool())
        .unwrap_or(res.requires_hardware_rt);
    res
}

/// Parses a primitive type name. Returns `None` for non-primitive strings.
pub fn parse_primitive(s: &str) -> Option<PrimitiveType> {
    match s {
        "i8" => Some(PrimitiveType::I8),
        "i16" => Some(PrimitiveType::I16),
        "i32" => Some(PrimitiveType::I32),
        "i64" => Some(PrimitiveType::I64),
        "u8" => Some(PrimitiveType::U8),
        "u16" => Some(PrimitiveType::U16),
        "u32" => Some(PrimitiveType::U32),
        "u64" => Some(PrimitiveType::U64),
        "f32" => Some(PrimitiveType::F32),
        "f64" => Some(PrimitiveType::F64),
        _ => None,
    }
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

fn require_hash<'a>(node: &'a Yaml, key: &str, diag: &mut Diagnostics) -> Option<&'a Yaml> {
    let child = lookup(node, key)?;
    if child.as_hash().is_some() {
        Some(child)
    } else {
        diag.push(key, "expected mapping");
        None
    }
}

fn lookup<'a>(node: &'a Yaml, key: &str) -> Option<&'a Yaml> {
    node.as_hash()
        .and_then(|h| h.get(&Yaml::String(key.to_owned())))
}

fn lookup_in_hash<'a>(hash: &'a yaml_rust2::yaml::Hash, key: &str) -> Option<&'a Yaml> {
    hash.get(&Yaml::String(key.to_owned()))
}

fn hash_get_string(
    hash: &yaml_rust2::yaml::Hash,
    key: &str,
    path: &str,
    diag: &mut Diagnostics,
) -> Option<String> {
    let value = lookup_in_hash(hash, key)?;
    if let Some(s) = value.as_str() {
        Some(s.to_owned())
    } else {
        diag.push(path, "expected string");
        None
    }
}

fn get_string(node: &Yaml, path: &str, diag: &mut Diagnostics) -> Option<String> {
    let value = lookup(node, "name")?;
    if let Some(s) = value.as_str() {
        Some(s.to_owned())
    } else {
        diag.push(path, "expected string");
        None
    }
}
