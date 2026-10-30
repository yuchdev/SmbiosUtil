# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run Tasks 01-02's tests together and close out Story 02.0 - the C API's
functional surface is complete at this point, modulo Story 03.0's
build/packaging work.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] All C API tests pass on real hardware across every platform
      milestone 0005 supports (Windows, Linux, macOS).
- [ ] A small end-to-end C program (not just C++ test files calling the
      C API) is written and run manually at least once, compiled with a
      plain C compiler and linked against `smbios_c`, proving the API is
      genuinely usable from C and not just from C++ pretending to be C -
      recorded in `status.md`.
- [ ] Milestone 0007's `status.md` Story 02.0 row is updated to ✅.
