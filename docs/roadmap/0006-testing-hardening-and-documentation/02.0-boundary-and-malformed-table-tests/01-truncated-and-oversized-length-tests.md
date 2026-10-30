# 01 - Truncated and oversized length tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Write the first three of R4's four boundary categories: a structure whose
declared `length` claims more bytes than remain in the table; a table
missing its terminating double-null string-section marker; and a table
whose declared/expected structure count (as `count_smbios_structures()`
computes it) disagrees with how many structures are actually walked
before hitting `EndOfTable` or running out of buffer.

## Files

- **Create** `tests/unit/malformed_table_test.cpp`:
  - `TruncatedLengthOverrunsBuffer`: build a table via
    `SyntheticTableBuilder::add_raw_bytes` whose last structure's `length`
    byte claims more than the remaining buffer size; assert
    `read_smbios_table()` stops at the prior structure, with no
    out-of-bounds read (confirmed by running under ASan).
  - `MissingDoubleNullTerminator`: a structure whose string section never
    hits `\0\0` before the buffer ends; assert `parse_dmi_strings()`
    (milestone 0003, hardened) stops cleanly rather than reading past
    `table_end`.
  - `StructureCountMismatch`: a table where `count_smbios_structures()`'s
    independent walk and `read_smbios_table()`'s walk would disagree
    (e.g. a structure the first walker's loop condition skips but the
    second's does not, or vice versa, exploiting the pre-existing
    duplication milestone 0003's Story 03.0 Task 01 flagged but did not
    deduplicate); assert both walkers still produce *internally
    consistent, safe* results even though they may legitimately disagree
    in structure count on this adversarial input - document the actual
    observed behavior rather than assuming they must agree.

## Functions and Classes

N/A - test-only.

## Constraints

- Every assertion in this task's tests is "does not read out of bounds /
  stops cleanly," not "produces some specific recovered output" - for
  adversarial input, correctness means safety, not a particular parse
  result.

## Tests

This task *is* the test file - no separate meta-test.

## Success criteria

- [ ] All three fixtures pass under the sanitizer CI job with zero
      ASan/UBSan findings.
- [ ] The `StructureCountMismatch` case's actual observed behavior (do
      the two walkers agree or not, on this specific adversarial input)
      is documented in the test's own comment and in `status.md`, since
      milestone 0003 left the two walkers' duplication as accepted
      pre-existing debt rather than unifying them.
