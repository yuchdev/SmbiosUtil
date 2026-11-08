# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Tasks 01-03

## Objective

Consolidated factory-resolution test for this story's 5 types (43, 44,
45, 46, 126) - the milestone's final verification pass. Once this task
lands, all 34 non-obsolete remaining SMBIOS structure types (per
plan.md's own enumeration) resolve through `SMBiosEntryFactory::create()`.

## Files

- `tests/unit/smbios_entry_factory_test.cpp` (or wherever the existing
  factory-resolution test lives - confirm exact file before editing):
  add cases for `SMBiosHandler` values 43, 44, 45, 46, 126.

## Functions and Classes

None new.

## Tests

- Factory resolves a non-null `AbstractSMBiosEntry` for each of the 5
  types given a minimally-valid synthetic header + buffer.
- Full `ctest` suite green with Tasks 01-03's per-type unit tests
  included.
- **Milestone-closing check**: a single consolidated test (or an
  extension of the existing one) iterating every `SMBiosHandler` value
  this milestone's plan.md names (2, 11-15, 18, 20-31, 33-46, 126) plus
  every value already implemented before this milestone (0, 1, 3, 4, 7,
  8, 9, 16, 17, 19, 32) confirms the factory returns non-null for all 45
  - leaving only 5, 6, 10 (Obsolete, Non-goals), 127 (EndOfTable, handled
  structurally by `read_smbios_table()` itself, never routed through the
  factory), and any true OEM-specific (0x80-0xFF) range unregistered, by
  design.

## Success criteria

- [ ] All 5 `SMBiosHandler` values (43, 44, 45, 46, 126) confirmed
      resolving through the factory in one consolidated test.
- [ ] Milestone-closing check (all 45 implemented types resolve; the 4
      deliberately-excluded values do not) passes.
- [ ] Full suite green; case count increase recorded in status.md.
- [ ] status.md updated to mark every story in this milestone ✅ Complete.
