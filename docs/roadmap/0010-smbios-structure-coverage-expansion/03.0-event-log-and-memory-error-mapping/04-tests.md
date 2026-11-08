# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Tasks 01-03

## Objective

Consolidated factory-resolution test for this story's 4 types (15, 18,
20, 33), matching milestone 0004 Story 02.0 Task 04's precedent.

## Files

- `tests/unit/smbios_entry_factory_test.cpp` (or wherever the existing
  factory-resolution test lives - confirm exact file before editing):
  add cases for `SMBiosHandler` values 15, 18, 20, 33.

## Functions and Classes

None new.

## Tests

- Factory resolves a non-null `AbstractSMBiosEntry` for each of the 4
  types given a minimally-valid synthetic header + buffer.
- Full `ctest` suite green with Tasks 01-03's per-type unit tests
  included.

## Success criteria

- [ ] All 4 `SMBiosHandler` values (15, 18, 20, 33) confirmed resolving
      through the factory in one consolidated test.
- [ ] Full suite green; case count increase recorded in status.md.
