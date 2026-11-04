# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01, Task 02

## Objective

Prove `cmake --build --preset macos` succeeds end-to-end for real -
configure, compile, **and link** - closing the gap the earlier,
syntax-only (`-fsyntax-only`) verification intentionally left open, and
confirm the resulting binaries behave as this story's Key constraints
predict (clean failure to find a table, not a crash).

## Files

No new test source - this task is a build/run verification gate, same
character as Story 01.0/02.0's own "Tests" tasks.

## Constraints

- `-fsyntax-only` (used to confirm Tasks 01/02's fix before writing their
  specs) proves every translation unit's syntax and semantics are
  correct, but not that the final link succeeds (symbol resolution,
  Boost.Iostreams' actual library archive, etc.) - this task is where
  that gap closes for real.
- Running `smbios_functional_test`/`smbios_util` without root on macOS is
  expected to either report no SMBIOS table found or throw a caught
  exception - **not** crash, hang, or corrupt memory. Use a sanitizer
  build (`-fsanitize=address,undefined`, consistent with milestone
  0002's planned sanitizer job, even though that job itself isn't wired
  up until that milestone) as an extra check that the "no source found"
  path is memory-safe, not just logically correct.

## Success criteria

- [ ] `cmake --preset macos && cmake --build --preset macos` succeeds
      completely on real macOS hardware - every target in `smbios` and
      `smbios_util` compiles **and links**, not just syntax-checks clean.
- [ ] Running the resulting `smbios_util`/`smbios_functional_test`
      binaries without elevated privileges does not crash - a clean
      "no SMBIOS table found" outcome (exception or empty state) is the
      correct, expected result per this story's own Key constraints, not
      a failure to chase further.
- [ ] A quick sanitizer-instrumented build/run of the same path reports
      no ASan/UBSan findings.
- [ ] Linux and Windows builds (already proven or expected-to-work before
      this story, per Story 02.0's own records) are unaffected - rerun
      the Linux/Windows CI jobs (once pushed) or, if still local-only at
      this point, re-confirm by code review that neither of this story's
      two diffs touches anything inside the GCC or WIN32 branches/guards.
- [ ] Milestone 0001's `status.md` Story 02.1 row is updated to ✅, and
      the CI workflow's macOS matrix entry (`allow_build_failure: true`,
      added in Story 02.0 Task 03 specifically because of this defect) is
      flipped back to `false` - since the known, tracked reason for
      tolerating a red macOS build no longer exists.
