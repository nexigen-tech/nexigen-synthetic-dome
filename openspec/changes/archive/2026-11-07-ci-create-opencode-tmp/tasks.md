# Tasks: Create /tmp/opencode in CI

## 1. Update CI workflow

- [x] 1.1 Add a dedicated step that creates `/tmp/opencode` as the runner user (no sudo) before tests run.
- [x] 1.2 Ensure the directory is writable by the test process.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the fix.
- [ ] 2.3 Confirm recording tests pass in the next CI run.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [ ] 3.2 Run `openspec archive` for `ci-create-opencode-tmp`.
