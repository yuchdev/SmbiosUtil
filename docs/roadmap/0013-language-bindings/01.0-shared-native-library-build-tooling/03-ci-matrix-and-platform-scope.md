# 03 - CI matrix and platform scope

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Define which OS/architecture combinations this milestone's CI actually
builds and tests both bindings against, consistent with Task 01's
acquisition-strategy decision (a platform this milestone cannot reliably
acquire a built `smbios_c` for is out of scope for this milestone's exit
criteria, not silently assumed to work), and wire `smbios_c` as a
same-workflow prerequisite build step both bindings' jobs depend on.

## Files

- **Modify** `.github/workflows/ci.yml`: add a job (or extend an
  existing one) that:
  1. Builds `smbios_c` via this project's own existing preset path
     (`cmake --preset <os>` + `cmake --build` + `cmake --install`,
     mirroring milestone 0007 Story 03.0's own CI verification) as a
     prerequisite step, OR downloads a GitHub Releases artifact per Task
     01's decision - whichever Task 01 actually chose as the CI-time
     mechanism (the two need not be the same mechanism end users get by
     default; CI can afford to always build from source for
     reproducibility even if end users get a prebuilt artifact).
  2. Runs `scripts/check_c_abi_binding_sync.py` (Task 02) as an early,
     fast-failing step before either binding's own build.
  3. Runs each binding's own build + test steps (added in Stories
     02.0/03.0), gated behind step 1/2 succeeding.

## Functions and Classes

N/A - CI/build wiring only.

## Constraints

- Mirror this project's existing CI platform set
  (`windows-latest`/`ubuntu-latest`/`macos-latest`, per `.github/workflows/ci.yml`'s
  current matrix) as the *ceiling* - do not add a platform this
  project's own C++ build does not already cover - but the *floor* for
  this milestone's own pass/fail gate may be narrower (e.g.
  Linux+macOS only, Windows warning-only) if Task 01 scoped Windows out
  for either binding; state which explicitly, do not leave it implicit
  in a matrix exclusion a future reader has to reverse-engineer.
- `smbios_c`'s own existing `tests/c_abi/` suite is unaffected and
  continues to run exactly as before - this task only adds new,
  additional steps/jobs, it does not restructure the existing C ABI
  tests.

## Tests

- N/A for this task itself - its own exit condition is "the CI job
  topology exists and the sync check runs early," verified by a green
  CI run once Stories 02.0-04.0 have actual build steps to gate.

## Success criteria

- [ ] The CI workflow change documents, in a comment or this repo's own
      docs, exactly which platforms are pass/fail-gating for this
      milestone versus warning-only.
- [ ] The sync check (Task 02) runs as an explicit, early, fast-failing
      step ahead of either binding's own build in the new job(s).
- [ ] No change to any existing, already-passing CI job's behavior.
