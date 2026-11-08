# Story 01.0 - Hermetic Fixture-Based Test Layer

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete

## Scope

Build `SyntheticTableBuilder` (contract, defined in Task 01), migrate the
ad hoc synthetic buffers from milestones 0003/0004's task specs onto it,
and wire a CI job (milestone 0001's workflow) that runs the full hermetic
suite unprivileged on every push - closing milestone 0001's Story 02.0
Task 03 "build-only" gap.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Synthetic table builder](01-synthetic-table-builder.md)               | ✅ Done | 02     |
| 02 | [Migrate functional tests to fixtures](02-migrate-functional-tests-to-fixtures.md) | ✅ Done | 03     |
| 03 | [Tests](03-tests.md)                                                  | ✅ Done | -      |

## Key constraints

- `SyntheticTableBuilder` constructs tables that feed directly into
  `SMBios::read_smbios_table()`/`count_smbios_structures()` - it does not
  go through any `ISmbiosSource` (milestone 0005), since the whole point
  is to test the parser independent of where bytes come from.
- The real-hardware functional suite stays (per the milestone's
  Non-goals) - this story adds a parallel, hermetic suite, it does not
  replace the existing one.

## Files or directories created or modified by this story

- `tests/unit/synthetic_table_builder.h` / `.cpp` (new; actual location -
  the spec's `smbios/test/unit/` path predates the repo-root
  restructuring, see CLAUDE.md).
- 13 `tests/unit/*_test.cpp` files that hand-rolled their own synthetic
  byte buffers - migrated onto the shared builder (R2); see status.md for
  the full list.
- `.github/workflows/ci.yml` - the unprivileged hermetic suite was
  already running per-OS via the `build` (Windows) and `sanitizers`
  (Linux/macOS) jobs' existing "Run unprivileged tests" steps; Task 03
  added a traceability comment rather than a new job, since adding a
  fourth near-duplicate job name would not have made the suite any more
  unprivileged or any more complete - see Task 03's own doc for the
  reasoning.
