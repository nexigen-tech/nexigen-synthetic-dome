# Design: Fix CI configure failures and add debug output

## Cache invalidation

The CMake dependency cache key previously used:

```yaml
key: ${{ runner.os }}-${{ matrix.name }}-deps-${{ hashFiles('CMakeLists.txt') }}
restore-keys: |
  ${{ runner.os }}-${{ matrix.name }}-deps-
```

The `restore-keys` prefix could match caches produced by failed configure runs, potentially restoring a corrupted `_deps` directory. Bump the prefix to `deps-v2-` to start fresh:

```yaml
key: ${{ runner.os }}-${{ matrix.name }}-deps-v2-${{ hashFiles('CMakeLists.txt') }}
restore-keys: |
  ${{ runner.os }}-${{ matrix.name }}-deps-v2-
```

## Configure debug output

Wrap the configure command to print `CMakeFiles/CMakeOutput.log` and `CMakeFiles/CMakeError.log` when CMake fails:

```yaml
- name: Configure
  run: |
    cmake -S . -B ${{ matrix.build-dir }} ${{ matrix.cmake-args }} || {
      echo "=== CMakeOutput.log ==="
      cat ${{ matrix.build-dir }}/CMakeFiles/CMakeOutput.log 2>/dev/null || true
      echo "=== CMakeError.log ==="
      cat ${{ matrix.build-dir }}/CMakeFiles/CMakeError.log 2>/dev/null || true
      exit 1
    }
```

This exposes the reason directly in the job log.
