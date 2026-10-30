# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the handle/error-model foundation is solid before Story 02.0
builds the real accessor surface on top of it, closing out Story 01.0.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] `compile_as_c_test.c` and `smbios_open_close_test.cpp` both pass.
- [ ] Sanitizer CI job (milestone 0002) is green against the new
      `smbios_c` target.
- [ ] Milestone 0007's `status.md` Story 01.0 row is updated to ✅.
