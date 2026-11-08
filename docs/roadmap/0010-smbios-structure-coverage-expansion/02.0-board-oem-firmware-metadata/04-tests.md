# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ✅ Done
**Depends on:** Tasks 01-03

## Objective

Consolidated verification that all 5 of this story's types (2, 11, 12,
13, 14) register and resolve correctly through
`SMBiosEntryFactory::create()`, matching milestone 0004 Story 02.0 Task
04's own precedent of one consolidated factory-resolution test per story.

## Files

- `tests/unit/smbios_entry_factory_test.cpp` (modify, if this is where the
  existing factory-resolution test lives - confirm exact file by grepping
  `SMBiosEntryFactory::create` under `tests/unit/`): add cases for
  `SMBiosHandler` values 2, 11, 12, 13, 14.

## Functions and Classes

None new - this task only adds test cases.

## Tests

- Factory resolves a non-null `AbstractSMBiosEntry` for each of the 5
  types given a minimally-valid synthetic header + buffer.
- Full `ctest` suite still green with the per-type unit tests from Tasks
  01-03 included.

## Success criteria

- [ ] All 5 `SMBiosHandler` values (2, 11, 12, 13, 14) confirmed resolving
      through the factory in one consolidated test.
- [ ] Full suite green, case count increased by exactly the number of new
      cases added across Tasks 01-04 (recorded in status.md).
