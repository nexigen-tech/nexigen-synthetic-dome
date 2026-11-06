# Tasks: Add configure debug output to CI workflow

## 1. Update workflow

- [x] 1.1 Bump CMake dependency cache key prefix to `deps-v2-` to avoid restoring broken caches.
- [x] 1.2 Wrap the configure step to print CMake logs on failure.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax.
- [x] 2.2 Commit and push.
- [x] 2.3 Inspect the next CI run.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [x] 3.2 Run `openspec archive` for `ci-add-configure-debug`.
