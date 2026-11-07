# Tasks: Install cbindgen in CI

## 1. Add cbindgen install step

- [x] 1.1 Add a CI step that installs `cbindgen` via `cargo install cbindgen` after the Rust toolchain.
- [x] 1.2 Place the step before the build so the `nxd_abi_header` and chronos header targets can run.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the fix.
- [x] 2.3 Confirm CI reaches the build and test steps.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [ ] 3.2 Run `openspec archive` for `ci-install-cbindgen`.
