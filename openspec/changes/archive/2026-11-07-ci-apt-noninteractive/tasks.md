# Tasks: Make apt-get non-interactive in CI

## 1. Update CI workflow

- [x] 1.1 Set `DEBIAN_FRONTEND=noninteractive` and `APT_LISTCHANGES_FRONTEND=none` for the system dependencies step.
- [x] 1.2 Add `-qq` to `apt-get update` and `apt-get install` to reduce output and avoid interactive prompts.

## 2. Validate and push

- [x] 2.1 Validate YAML syntax of `.github/workflows/ci.yml`.
- [x] 2.2 Commit and push the fix.
- [ ] 2.3 Confirm CI no longer hangs on the system dependencies step.

## 3. Archive change

- [x] 3.1 Mark tasks complete.
- [ ] 3.2 Run `openspec archive` for `ci-apt-noninteractive`.
