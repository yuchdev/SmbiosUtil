# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the presets and CI workflow from this story reproduce the same
binaries and test results as the raw command lines CLAUDE.md documents
today, on all three platforms, closing out milestone 0001.

## Files

No new test source - this is the milestone's exit gate.

## Functions and Classes

N/A.

## Success criteria

- [ ] `cmake --preset <os> && cmake --build --preset <os>` on Windows,
      Linux and macOS produces `smbios_functional_test` and
      `smbios/test/performance_test` binaries in `bin/` as documented.
- [ ] CI is green on all three OS jobs for a no-op commit (proving the
      workflow itself, not a feature, is the thing being validated).
- [ ] `grep -r "WINDOWS_BOOST_DIR\|MY_BOOST_DIR" .` (repo-wide) returns no
      matches, and `docs/dev/building.md`/CLAUDE.md no longer mention them.
- [ ] Milestone 0001's `status.md` is updated to ✅ for both stories, with a
      closeout note listing the CI run that proved it.
