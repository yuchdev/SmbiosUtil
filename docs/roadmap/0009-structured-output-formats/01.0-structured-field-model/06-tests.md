# 06 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 05

## Objective

Run every entry type's field tests together and close out Story 01.0,
before Story 02.0's renderers are built against the now-complete,
compiler-enforced field model.

## Files

This task adds no test source beyond Tasks 01-05's additions. It is the
story's exit gate.

## Success criteria

- [ ] All 45 entry types' `get_fields()` tests pass in `smbios_unit_test`,
      including Task 05's coverage case.
- [ ] `get_fields()` is pure virtual, and no `TEMPORARY` default remains.
- [ ] `smbios_unit_test` is green on all three OSes in CI (`build` job on
      Windows, `sanitizers` job on Linux/macOS).
- [ ] The existing integration suite (`smbios_integration_test`, text
      output path, unaffected by this story) passes unmodified on real
      hardware.
- [ ] Milestone 0009's `status.md` Story 01.0 row is updated to ✅.
