# 02 - Tests

**Parent story:** [README.md](README.md)
**Status:** ✅ Done
**Depends on:** Task 01

## Objective

Verify, by running (not merely building) the full existing test suite,
that the `tables/` relocation (Task 01) changed zero behavior - satisfying
ADR 0002's DD3 and this milestone's R1.

## Files

- No new test files. This task is a verification pass over the existing
  suite plus whatever `#include` edits Task 01 made to test files.

## Functions and Classes

None.

## Constraints

- No test assertion changes - if any assertion needed to change for the
  suite to pass, that is evidence Task 01 introduced a behavior change,
  which must be investigated and fixed in Task 01, not patched over here
  by adjusting the expected value.
- Run on at least one platform via `ctest`, not just a build-success
  check - matching this project's own established bar (e.g. milestone
  0005's rollout note: "verified the same way this milestone already
  verifies every other story").

## Tests

- `ctest --test-dir build/<preset> --output-on-failure` - all 4 targets
  (`smbios_unit_test`, `smbios_mock_test`, `smbios_perf_test`,
  `smbios_integration_test`) green, same pass/case counts as immediately
  before this story started (record the baseline count before Task 01
  begins, compare after).

## Success criteria

- [ ] Full `ctest` suite passes with the same case count as the
      pre-story baseline.
- [ ] No test assertion was modified; only `#include` paths changed in any
      test file that needed updating.

**Verification result (2026-11-08):** `ctest --test-dir build/macos --output-on-failure` - 4/4 targets green, `smbios_unit_test` 166 cases (identical to pre-story baseline). `git show` of the relocation commit confirms exactly 1-line `#include`-path changes in each of the 13 touched test files - no assertion was modified.
