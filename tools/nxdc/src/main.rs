//! NXD schema compiler (`nxdc`) – canonical IDL parser and binding generator.
//!
//! ABI 1 components use `panic = "abort"` so no Rust panic unwinds across the
//! C plugin boundary.  The compiler itself is cold-path tooling: parsing,
//! validation, and emission may allocate and format freely.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        print_usage();
        return ExitCode::FAILURE;
    }

    let command = &args[1];
    let path = Path::new(&args[2]);

    match command.as_str() {
        "validate-contract" => validate_contract(path),
        "validate-component" => validate_component(path),
        "canonicalize" => canonicalize(path, args.get(3).map(PathBuf::from)),
        "emit-component-descriptor" => {
            emit_component_descriptor(path, args.get(3).map(PathBuf::from))
        }
        "emit-cpp" => emit_cpp(path, args.get(3).map(PathBuf::from)),
        "emit-rust" => emit_rust(path, args.get(3).map(PathBuf::from)),
        "emit-descriptors" => emit_descriptors(path, args.get(3).map(PathBuf::from)),
        _ => {
            eprintln!("unknown command: {command}");
            print_usage();
            ExitCode::FAILURE
        }
    }
}

fn print_usage() {
    eprintln!("usage: nxdc <command> <file.yaml> [<output>]");
    eprintln!("commands:");
    eprintln!("  validate-contract          load and validate a contract schema");
    eprintln!("  validate-component         load and validate a component descriptor");
    eprintln!("  canonicalize               emit canonical JSON and SHA-256 layout hash");
    eprintln!("  emit-component-descriptor  emit canonical component descriptor JSON and hash");
    eprintln!("  emit-cpp                   emit C++23 header(s) to file or directory");
    eprintln!("  emit-rust                  emit Rust source(s) to file or directory");
    eprintln!("  emit-descriptors           emit field-descriptor JSON(s) to file or directory");
}

fn validate_contract(path: &Path) -> ExitCode {
    match nxdc::load_contract(path) {
        Ok(schema) => {
            println!(
                "contract '{}' version {} loaded with {} type(s) and {} field(s)",
                schema.name,
                schema.version,
                schema.types.len(),
                schema.fields.len()
            );
            ExitCode::SUCCESS
        }
        Err(diag) => {
            eprintln!("{diag}");
            ExitCode::FAILURE
        }
    }
}

fn validate_component(path: &Path) -> ExitCode {
    match nxdc::load_component(path) {
        Ok(descriptor) => {
            println!(
                "component '{}' version {} loaded with {} port(s) and {} parameter(s)",
                descriptor.name,
                descriptor.version,
                descriptor.ports.len(),
                descriptor.parameters.len()
            );
            ExitCode::SUCCESS
        }
        Err(diag) => {
            eprintln!("{diag}");
            ExitCode::FAILURE
        }
    }
}

fn canonicalize(path: &Path, output: Option<PathBuf>) -> ExitCode {
    let schemas = match nxdc::load_concrete_contracts(path) {
        Ok(s) => s,
        Err(diag) => {
            eprintln!("{diag}");
            return ExitCode::FAILURE;
        }
    };

    for schema in &schemas {
        let (json, hash) = match nxdc::canonical::emit(schema) {
            Ok(r) => r,
            Err(e) => {
                eprintln!("canonicalization failed for {}: {e}", schema.name);
                return ExitCode::FAILURE;
            }
        };

        if let Some(out_path) = &output {
            let final_path = if schemas.len() == 1 {
                out_path.clone()
            } else {
                out_path.join(format!("{}.canonical.json", schema.name))
            };
            if let Some(parent) = final_path.parent() {
                let _ = fs::create_dir_all(parent);
            }
            if let Err(e) = fs::write(&final_path, json) {
                eprintln!("failed to write {}: {e}", final_path.display());
                return ExitCode::FAILURE;
            }
            println!("{} -> {}", schema.name, final_path.display());
        } else {
            println!("{json}");
        }

        println!(
            "layout_hash: {}",
            hash.iter().map(|b| format!("{b:02x}")).collect::<String>()
        );
    }
    ExitCode::SUCCESS
}

/// Finds the project `contracts/` directory by walking up from `component_path`
/// and looking for a directory named `contracts`.
fn find_contracts_dir(component_path: &Path) -> Option<PathBuf> {
    let mut dir = component_path.parent()?;
    loop {
        let candidate = dir.join("contracts");
        if candidate.is_dir() {
            return Some(candidate);
        }
        dir = dir.parent()?;
    }
}

/// Loads every contract YAML in `contracts_dir`, computes the SHA-256 layout
/// hash for each concrete contract, and fills in missing `layout_hash` values
/// on `descriptor.ports` by looking up the port's `contract_name`.
fn fill_port_layout_hashes(
    descriptor: &mut nxdc::model::ComponentDescriptor,
    component_path: &Path,
) {
    let contracts_dir = match find_contracts_dir(component_path) {
        Some(d) => d,
        None => return,
    };

    let mut hashes: std::collections::HashMap<String, String> = std::collections::HashMap::new();
    let entries = match fs::read_dir(&contracts_dir) {
        Ok(e) => e,
        Err(_) => return,
    };

    for entry in entries.flatten() {
        let path = entry.path();
        if path.extension().and_then(|e| e.to_str()) != Some("yaml") {
            continue;
        }
        let schemas = match nxdc::load_concrete_contracts(&path) {
            Ok(s) => s,
            Err(_) => continue,
        };
        for schema in schemas {
            if let Ok((_, hash)) = nxdc::canonical::emit(&schema) {
                let hash_hex = hash.iter().map(|b| format!("{b:02x}")).collect::<String>();
                hashes.insert(schema.name.clone(), hash_hex);
            }
        }
    }

    for port in &mut descriptor.ports {
        if port.layout_hash.is_none() {
            if let Some(hash) = hashes.get(&port.contract_name) {
                port.layout_hash = Some(hash.clone());
            }
        }
    }
}

fn emit_component_descriptor(path: &Path, output: Option<PathBuf>) -> ExitCode {
    let mut descriptor = match nxdc::load_component(path) {
        Ok(d) => d,
        Err(diag) => {
            eprintln!("{diag}");
            return ExitCode::FAILURE;
        }
    };

    fill_port_layout_hashes(&mut descriptor, path);

    let (json, hash) = match nxdc::canonical::emit_component(&descriptor) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("component descriptor canonicalization failed: {e}");
            return ExitCode::FAILURE;
        }
    };

    if let Some(out_path) = &output {
        if let Some(parent) = out_path.parent() {
            let _ = fs::create_dir_all(parent);
        }
        if let Err(e) = fs::write(out_path, &json) {
            eprintln!("failed to write {}: {e}", out_path.display());
            return ExitCode::FAILURE;
        }
        let hash_path = out_path.with_extension("hash");
        let hash_text = hash.iter().map(|b| format!("{b:02x}")).collect::<String>();
        if let Err(e) = fs::write(&hash_path, hash_text) {
            eprintln!("failed to write {}: {e}", hash_path.display());
            return ExitCode::FAILURE;
        }
        println!("{} -> {}", descriptor.name, out_path.display());
    } else {
        println!("{json}");
    }

    println!(
        "descriptor_hash: {}",
        hash.iter().map(|b| format!("{b:02x}")).collect::<String>()
    );
    ExitCode::SUCCESS
}

fn emit_cpp(path: &Path, output: Option<PathBuf>) -> ExitCode {
    emit_each(path, output, "hpp", nxdc::gen::cpp::emit, "cpp")
}

fn emit_rust(path: &Path, output: Option<PathBuf>) -> ExitCode {
    emit_each(path, output, "rs", nxdc::gen::rust::emit, "rust")
}

fn emit_descriptors(path: &Path, output: Option<PathBuf>) -> ExitCode {
    emit_each(
        path,
        output,
        "descriptor.json",
        nxdc::gen::descriptor::emit,
        "descriptor",
    )
}

fn emit_each(
    path: &Path,
    output: Option<PathBuf>,
    extension: &str,
    emit_fn: fn(&nxdc::model::ContractSchema) -> Result<String, nxdc::diag::Diagnostics>,
    label: &str,
) -> ExitCode {
    let schemas = match nxdc::load_concrete_contracts(path) {
        Ok(s) => s,
        Err(diag) => {
            eprintln!("{diag}");
            return ExitCode::FAILURE;
        }
    };

    for schema in &schemas {
        let text = match emit_fn(schema) {
            Ok(t) => t,
            Err(diag) => {
                eprintln!("{label} emission failed for {}:\n{diag}", schema.name);
                return ExitCode::FAILURE;
            }
        };

        let final_path = match &output {
            Some(out) if schemas.len() == 1 => out.clone(),
            Some(out) => out.join(format!("{}.{extension}", schema.name)),
            None => {
                println!("{text}");
                continue;
            }
        };

        if let Some(parent) = final_path.parent() {
            let _ = fs::create_dir_all(parent);
        }
        if let Err(e) = fs::write(&final_path, text) {
            eprintln!("failed to write {}: {e}", final_path.display());
            return ExitCode::FAILURE;
        }
        println!("{} -> {}", schema.name, final_path.display());
    }
    ExitCode::SUCCESS
}
