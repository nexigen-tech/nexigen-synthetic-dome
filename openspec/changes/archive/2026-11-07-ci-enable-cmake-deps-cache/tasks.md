# Tasks: Re-enable CMake _deps cache in CI

## 1. Update CI workflow

- [x] 1.1 Uncomment the CMake dependency cache block.
- [x] 1.2 Keep the `deps-v2-` key prefix.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the change.
- [ ] 2.3 Confirm CI still passes with the cache enabled.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [ ] 3.2 Run `openspec archive` for `ci-enable-cmake-deps-cache`.
