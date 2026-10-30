# Story 01.0 - Hermetic Fixture-Based Test Layer

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Build `SyntheticTableBuilder` (contract, defined in Task 01), migrate the
ad hoc synthetic buffers from milestones 0003/0004's task specs onto it,
and wire a CI job (milestone 0001's workflow) that runs the full hermetic
suite unprivileged on every push - closing milestone 0001's Story 02.0
Task 03 "build-only" gap.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Synthetic table builder](01-synthetic-table-builder.md)               | ⬜ Not started | 02     |
| 02 | [Migrate functional tests to fixtures](02-migrate-functional-tests-to-fixtures.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- `SyntheticTableBuilder` constructs tables that feed directly into
  `SMBios::read_smbios_table()`/`count_smbios_structures()` - it does not
  go through any `ISmbiosSource` (milestone 0005), since the whole point
  is to test the parser independent of where bytes come from.
- The real-hardware functional suite stays (per the milestone's
  Non-goals) - this story adds a parallel, hermetic suite, it does not
  replace the existing one.

## Files or directories created or modified by this story

- `smbios/test/unit/synthetic_table_builder.h` / `.cpp` (new).
- Every `tests/unit/*_test.cpp` file created across milestones 0002-0005
  that hand-rolled its own synthetic byte array - migrated to use the
  shared builder (R2).
- `.github/workflows/ci.yml` (milestone 0001) - new unprivileged hermetic-
  suite job.
