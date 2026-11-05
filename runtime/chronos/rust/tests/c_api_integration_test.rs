//! Integration test: load the Chronos C API shared library from Rust and
//! exercise the full lifecycle of a runtime instance through the FFI table.

use libloading::{Library, Symbol};
use nxd_runtime_chronos::c_abi::{
    NxdChronosApiTable, NxdChronosConfig, NxdChronosInstance, NxdChronosMode, NxdChronosState,
    NXD_CHRONOS_OK,
};
use std::path::PathBuf;

fn shared_library_path() -> PathBuf {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let workspace_root = manifest_dir
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .parent()
        .unwrap();

    let build_dir = if cfg!(debug_assertions) {
        "build-debug"
    } else {
        "build-release"
    };

    workspace_root
        .join(build_dir)
        .join("runtime/chronos")
        .join("libnxd_chronos_capi.so")
}

#[test]
fn rust_can_drive_chronos_c_api() {
    unsafe {
        let lib =
            Library::new(shared_library_path()).expect("failed to load Chronos C API library");

        let entry: Symbol<unsafe extern "C" fn() -> *const NxdChronosApiTable> = lib
            .get(b"nxd_chronos_api\0")
            .expect("missing nxd_chronos_api symbol");
        let table = entry();
        assert!(!table.is_null(), "nxd_chronos_api returned null");

        let table = &*table;
        assert_eq!(table.version_major, 1);
        assert_eq!(table.table_size, std::mem::size_of::<NxdChronosApiTable>());

        let config = NxdChronosConfig {
            mode: NxdChronosMode::Step,
            base_frame_interval_ns: 1_000_000,
            scale_factor: 1,
        };

        let instance: *mut NxdChronosInstance = (table.create)(&config);
        assert!(!instance.is_null(), "create returned null");

        assert_eq!(
            (table.initialize)(instance),
            NXD_CHRONOS_OK,
            "initialize failed"
        );
        assert_eq!(
            (table.get_state)(instance),
            NxdChronosState::Initialized,
            "state after initialize"
        );

        assert_eq!((table.arm)(instance), NXD_CHRONOS_OK, "arm failed");
        assert_eq!(
            (table.get_state)(instance),
            NxdChronosState::Armed,
            "state after arm"
        );

        assert_eq!((table.step)(instance), NXD_CHRONOS_OK, "step failed");
        assert_eq!(
            (table.get_state)(instance),
            NxdChronosState::Armed,
            "state after step"
        );
        assert_eq!(
            (table.get_frame_no)(instance),
            1,
            "frame number after one step"
        );
        assert_eq!(
            (table.get_sim_time_ns)(instance),
            1_000_000,
            "sim time after one step"
        );

        assert_eq!((table.stop)(instance), NXD_CHRONOS_OK, "stop failed");
        assert_eq!(
            (table.get_state)(instance),
            NxdChronosState::Stopped,
            "state after stop"
        );

        assert_eq!((table.dispose)(instance), NXD_CHRONOS_OK, "dispose failed");
        assert_eq!(
            (table.get_state)(instance),
            NxdChronosState::Disposed,
            "state after dispose"
        );

        (table.destroy)(instance);
    }
}
