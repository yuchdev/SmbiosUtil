# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the accessor utility and its first real call site (`DMIHeader`
extraction) are correct before Story 02.0 builds the rest of the entry
model on top of them.

## Files

No new test source beyond Task 01's `byte_reader_test.cpp` - this task is
the story's exit gate.

## Success criteria

- [ ] `byte_reader_test.cpp` passes (Task 01's success criteria).
- [ ] Full existing functional suite passes unmodified on real hardware
      after Task 02's `DMIHeader` change.
- [ ] Milestone 0003's `status.md` Story 01.0 row is updated to ✅.
