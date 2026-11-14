# 01 - Freeze the check inventory and measure the log baseline

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13)

## Objective

Make `plan.md`'s Phase-0 inventory (C01-C22) exactly true for the `ci.yml` at the commit where the work starts, and record numbers for the "before" state, so that the exit gate can show an improvement instead of asserting one.

## Files

- **Modify** this milestone's `plan.md`, Phase-0 table, only if drift is found. Record each drift as `<ID>: expected <as written> -> actual <as found>`, and add any step missing from the table as a new ID (C23 onward).
- **Modify** this milestone's `status.md`: add a "Baseline" note with the numbers below.

## Functions and Classes

None.

## Constraints

- **Read-only on CI.** Nothing is pushed. The baseline comes from existing runs: the last green full run (milestone 0011's run 43083659559, or a later one), and one failing run if any is still retained (for example 43067987979, the deliberate `gui-ci` failure).
- Measure with `gh run view <run-id> --log` per job. Record counts only: bytes, lines and durations. Do not paste log content into the docs.
- **Confirm the two "found by reading" items.**
  - C05: no CMake target named `smbios_functional_test` exists in the tree; the step's log line on a green run shows the "not found" warning.
  - E11: the sanitizer configure line has no `-fno-sanitize-recover`, and the job sets no `UBSAN_OPTIONS`.

## Tests

- Cross-check that every `- name:` step in `ci.yml` maps to exactly one inventory ID, apart from checkout and pure setup. The count of steps without an ID must be 0.

## Success criteria

- [x] Inventory table matches `ci.yml` at HEAD. Drift is recorded in `expected -> actual` form, or "no drift" is recorded.
- [x] `status.md` records, per job and leg, for one green and (if retained) one failing run: total log bytes, total log lines, the lines produced by project-owned `run:` steps versus third-party actions, and the job duration. It also records the median duration of the last 5 green runs per job (input to Story 02.0 Task 03's step budgets).
- [x] C05 and E11 are confirmed or withdrawn, in writing.
