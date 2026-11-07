# Tasks: Use Clang 19 in CI

## 1. Update CI workflow

- [x] 1.1 Install `clang-19` and `libstdc++-13-dev` instead of default `clang`.
- [x] 1.2 Update the `clang` matrix `cmake-args` to use `clang-19`/`clang++-19`.
- [x] 1.3 Add `~/.cargo/bin` to the Cargo cache so `cbindgen` is reused across jobs.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the fix.
- [ ] 2.3 Confirm CI `clang` job builds and tests.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [ ] 3.2 Run `openspec archive` for `ci-use-clang-19`.
