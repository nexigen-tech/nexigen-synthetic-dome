# Tasks: Fix CI workflow — clone missing nexigen-cpp dependency

## 1. Fix workflow

- [x] 1.1 Add a step to shallow-clone `nexigen-cpp` into `../nexigen-ecosystem/nexigen-cpp` before CMake configure.
- [x] 1.2 Broaden failure artifact upload to include CMake configure logs.
- [x] 1.3 Set `if-no-files-found: ignore` on the artifact upload step.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the fix.
- [x] 2.3 Confirm CI triggers and reaches the test step in all seven jobs.

## 3. Archive change

- [x] 3.1 Mark all tasks complete.
- [x] 3.2 Run `openspec archive` for `fix-ci-nexigen-cpp-dependency`.
