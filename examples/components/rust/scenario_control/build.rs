use std::env;
use std::path::Path;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR");
    let manifest_path = Path::new(&manifest_dir);

    let candidates = [
        "../../../../build-debug/runtime/blackboard",
        "../../../../build-release/runtime/blackboard",
        "../../../../build/runtime/blackboard",
    ];

    for candidate in &candidates {
        let lib_dir = manifest_path.join(candidate);
        if lib_dir.join("libnxd_blackboard.so").exists() {
            let lib_dir_str = lib_dir.to_str().expect("valid UTF-8 path");
            println!("cargo:rustc-link-search=native={}", lib_dir_str);
            println!("cargo:rustc-link-lib=dylib=nxd_blackboard");
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir_str);
            break;
        }
    }
}
