# 02 - Migrate functional tests to fixtures

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Satisfy R2: find every test added across milestones 0002-0005 that
constructed its own ad hoc synthetic byte buffer inline, and rewrite it
against `SyntheticTableBuilder` instead, so the construction logic lives
in one place.

## Files

- **Modify** (rewrite construction only, not assertions):
  `tests/unit/bios_information_entry_test.cpp`,
  `tests/unit/memory_device_entry_test.cpp`,
  `tests/unit/port_connection_entry_test.cpp`,
  `tests/unit/system_information_entry_test.cpp`,
  `tests/unit/system_enclosure_entry_test.cpp`,
  `tests/unit/processor_information_entry_test.cpp`,
  `tests/unit/cache_information_entry_test.cpp`,
  `tests/unit/system_slots_entry_test.cpp`,
  `tests/unit/physical_memory_array_entry_test.cpp`,
  `tests/unit/memory_array_mapped_address_entry_test.cpp`,
  `tests/unit/system_boot_information_entry_test.cpp`,
  `tests/unit/smbios_table_walk_test.cpp`,
  `tests/unit/dmi_string_parsing_test.cpp`
  (the exact file list depends on what milestones 0003/0004 actually
  named their test files - reconcile against what exists on disk at the
  time this task starts, rather than treating this list as gospel).

## Functions and Classes

N/A - test-construction rewrite only; no production code changes.

## Constraints

- Assertions (the expected decoded values) do not change - only how the
  input byte buffer is constructed. If a migrated test's assertions
  need to change to keep passing, that is a sign the original ad hoc
  buffer had a bug `SyntheticTableBuilder` is now exposing - investigate
  and fix the production code if so, do not silently adjust the
  assertion to match a wrong buffer.

## Tests

This task *is* the test changes - no separate test file.

## Success criteria

- [ ] Every test file listed above (or its as-built equivalent) uses
      `SyntheticTableBuilder` for input construction.
- [ ] `grep -rn "std::vector<uint8_t>.*{0x" tests/unit/` (a rough proxy for
      "hand-rolled byte array literal") shows a large reduction versus
      before this task - remaining hits should be `SyntheticTableBuilder`'s
      own internals or Story 02.0's deliberately-malformed fixtures, not
      duplicated well-formed-table construction.
