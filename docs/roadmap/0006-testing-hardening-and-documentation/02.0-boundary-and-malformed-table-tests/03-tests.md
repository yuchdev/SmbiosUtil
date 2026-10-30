# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run Tasks 01-02's fixtures and harness together under the sanitizer CI
job, and close out Story 02.0.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] `malformed_table_test.cpp`'s three fixtures pass under ASan/UBSan.
- [ ] The fuzz harness builds on Clang and has been run locally at least
      once with a recorded, clean (or fixed) result.
- [ ] Milestone 0006's `status.md` Story 02.0 row is updated to ✅.
