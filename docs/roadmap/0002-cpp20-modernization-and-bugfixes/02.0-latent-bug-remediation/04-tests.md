# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Collect the three regression tests from Tasks 01-03 into the test suite,
run the full existing functional suite to confirm no behavior regressed on
real hardware, and close out milestone 0002.

## Files

- **Modify/create** the appropriate test file(s) under
  `smbios/test/functional_test/` to house the three new regression tests
  (`SMBiosVersion` comparison, `get_table_size()`, and a smoke check that
  `smbios.cpp` compiles without the dead include is implicitly proven by
  the build itself, not a runtime test).

## Success criteria

- [ ] All three regression tests from Tasks 01-03 are present and pass.
- [ ] Full existing functional suite passes unmodified on real hardware
      (`--run_test=SmbiosFunctionalTests/SMBiosCreationTestCase` and the
      rest), proving the version-comparison fix did not change any
      currently-tested entry's rendered output.
- [ ] Sanitizer CI job (Story 01.0) is still green after these fixes.
- [ ] Milestone 0002's `status.md` is updated to ✅ for both stories, with
      a closeout note. CLAUDE.md's "Known rough edges" paragraph is updated
      to remove every bullet this milestone fixed.
