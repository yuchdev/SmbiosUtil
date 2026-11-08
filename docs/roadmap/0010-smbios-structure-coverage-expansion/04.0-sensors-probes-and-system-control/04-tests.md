# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Tasks 01-03

## Objective

Consolidated factory-resolution test for this story's 9 types (21-29),
matching milestone 0004 Story 02.0 Task 04's precedent.

## Files

- `tests/unit/smbios_entry_factory_test.cpp` (or wherever the existing
  factory-resolution test lives - confirm exact file before editing):
  add cases for `SMBiosHandler` values 21, 22, 23, 24, 25, 26, 27, 28, 29.

## Functions and Classes

None new.

## Tests

- Factory resolves a non-null `AbstractSMBiosEntry` for each of the 9
  types given a minimally-valid synthetic header + buffer.
- Full `ctest` suite green with Tasks 01-03's per-type unit tests
  included.

## Success criteria

- [ ] All 9 `SMBiosHandler` values (21-29) confirmed resolving through
      the factory in one consolidated test.
- [ ] Full suite green; case count increase recorded in status.md.
