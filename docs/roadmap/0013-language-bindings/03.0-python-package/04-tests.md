# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 03

## Objective

Close Story 03.0's exit gate with FFI-boundary-specific tests for the
Python package itself (unit-level, within `bindings/python/`), run in
CI - distinct from Story 04.0's cross-language, cross-binding parity
suite.

## Scope

- `smbios._ffi`: the raw-symbol-loading and sync-check tests from Task
  01, wired into `pytest` and CI.
- `smbios` public surface: the context-manager/exception-mapping/
  use-after-close tests from Task 02, wired into `pytest` and CI, run
  against the just-built wheel artifact (not an editable source install,
  to also catch a packaging regression - e.g. a forgotten bundled
  library), on every platform Story 01.0 Task 03 scoped as pass/fail
  gating, tolerant of a `NoTableFoundError` on a runner with no
  accessible SMBIOS table.
- A `mypy`/type-stub check (the package should ship a `py.typed` marker
  and accurate stubs, since a C-backed Python package with no type
  hints is a common source of consumer confusion) and a linter gate
  (e.g. `ruff`), matching this project's own C++ `-Wall -Wextra -Werror`
  hygiene bar applied to the Python side.

## Success criteria

- [ ] `pytest` passes against the built wheel artifact in CI on every
      platform Story 01.0 Task 03 scoped as pass/fail gating.
- [ ] The use-after-close regression test (Task 02) runs as part of this
      gate.
- [ ] `mypy`/`ruff` (or chosen equivalents) are clean.
- [ ] A missing/inaccessible real SMBIOS table on a CI runner produces a
      clean `NoTableFoundError`, not a test crash or hang.
