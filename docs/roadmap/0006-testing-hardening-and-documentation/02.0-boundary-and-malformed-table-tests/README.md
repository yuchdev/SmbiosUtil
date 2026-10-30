# Story 02.0 - Boundary and Malformed-Table Tests

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`SyntheticTableBuilder`'s `add_raw_bytes`)

## Scope

Write the four boundary-condition test categories R4 names - oversized
declared `length`, missing double-null terminator, structure-count
mismatch, and a fuzz harness entry point - proving milestone 0003's Story
03.0 hardening (`read_smbios_table`/`count_smbios_structures`/
`parse_dmi_strings` bounds checks) actually holds against adversarial
input, not just the hand-picked fixtures that story's own tasks wrote.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [Truncated and oversized length tests](01-truncated-and-oversized-length-tests.md) | ⬜ Not started | 03     |
| 02 | [Fuzz harness](02-fuzz-harness.md)                                | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                              | ⬜ Not started | -      |

## Key constraints

- Every test in this story runs under the sanitizer CI job (milestone
  0002's Story 01.0) - a boundary test that merely "doesn't crash" without
  sanitizer instrumentation proves much less than one that also reports
  zero ASan/UBSan findings.
- This story's fuzz harness (R4) is deliberately scoped as a local,
  documented tool, not continuous infrastructure - see the milestone's
  Non-goals.

## Files or directories created or modified by this story

- `tests/unit/malformed_table_test.cpp` (new).
- `smbios/test/fuzz/read_smbios_table_fuzzer.cpp` (new) - libFuzzer-style
  entry point, built only when an explicit CMake option
  (`-DSMBIOS_BUILD_FUZZERS=ON`, off by default) is set, so it never
  affects the default build.
