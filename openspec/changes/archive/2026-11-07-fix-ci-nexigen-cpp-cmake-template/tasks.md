# Tasks: Patch missing nexigen-cpp cmake config template in CI

## 1. Patch workflow

- [x] 1.1 Add a CI step that writes `cmake/nexigenConfig.cmake.in` into the cloned `nexigen-cpp` directory.
- [x] 1.2 Include a TODO comment explaining the upstream fix needed.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax.
- [x] 2.2 Commit and push.
- [x] 2.3 Verify CI reaches build and test steps.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [x] 3.2 Run `openspec archive` for `fix-ci-nexigen-cpp-cmake-template`.
