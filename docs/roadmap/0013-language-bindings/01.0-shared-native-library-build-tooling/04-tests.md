# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 03

## Objective

Close this story's exit gate: prove the sync-detection mechanism
(Task 02) and the CI wiring (Task 03) actually work together, end to
end, before either binding exists to consume them - so a defect in the
shared tooling itself is caught now, not discovered later while
debugging an apparently-unrelated Rust or Python build failure.

## Scope

- Run `scripts/check_c_abi_binding_sync.py` against the fixture pair
  from Task 02 as part of this story's own CI job (not just as a local
  manual check), confirming the "matches" fixture passes and both
  "drift" fixtures fail with the expected diff output, in CI, not just
  on a contributor's own machine.
- Confirm the CI job topology from Task 03 actually gates on the sync
  check's exit code - a deliberately-broken fixture committed to a
  scratch branch (or a CI-only `if: false` dry run documented in the
  task's own notes) must cause the job to fail, not just print a
  warning and continue.
- Confirm `smbios_c`'s own existing test suite (`tests/c_abi/`,
  milestone 0007) is unaffected: existing `ctest` pass/fail counts are
  identical before and after this story's CI changes land.

## Success criteria

- [ ] The sync check's fixture-based tests (Task 02) run in CI, not only
      locally.
- [ ] A deliberately broken fixture is confirmed to fail the CI job, not
      merely warn.
- [ ] `smbios_c`'s existing `tests/c_abi/` suite shows no regression in
      pass/fail count after this story's CI changes.
