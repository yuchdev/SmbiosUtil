# 03 - Cross-language round-trip parity

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Prove that, against the same SMBIOS source (real hardware, or - if
Story 01.0's own acquisition/test tooling makes one available - a
synthetic fixture), all three vantage points onto `smbios_c` agree
exactly: a direct C program calling `smbios_c` functions, the Rust
wrapper, and the Python wrapper. This is the strongest possible evidence
that neither binding has introduced a marshaling bug (string encoding,
buffer-length-retry off-by-one, integer width mismatch) that a
same-language unit test (Story 02.0/03.0's own Task 04) could not catch
by construction, since those only ever compare a binding against
itself.

## Files

- **Create** `scripts/compare_binding_round_trip.py` (or a Rust/Python
  test-runner-native equivalent, if simpler to keep the comparison logic
  inside one of the two languages' own test suites and have the other
  shell out to it): runs, in sequence, against the same live SMBIOS
  table:
  1. `tests/c_abi/smbios_c_example.c`-equivalent direct output (reusing
     milestone 0007's own existing example program, not a new parallel C
     program, to avoid a fourth implementation to keep in sync) -
     entry count, and for each entry: type/length/handle\_id, rendered
     description, and any of the five curated getters applicable to that
     entry's type.
  2. The equivalent sequence through the Rust wrapper.
  3. The equivalent sequence through the Python wrapper.
  4. A byte-for-byte diff of all three outputs; any difference is a
     reported test failure naming the exact field and entry index where
     the three disagreed.

## Functions and Classes

- `compare_binding_round_trip.py`'s own structure (names only):
  - `run_c_reference() -> list[dict]`
  - `run_rust_binding() -> list[dict]`
  - `run_python_binding() -> list[dict]`
  - `diff_results(c, rust, python) -> list[str]` (empty on full
    agreement)
  - `main() -> int`

## Constraints

- The comparison must include every entry in the live table, not a
  sample - a marshaling bug could plausibly affect only entries past a
  certain buffer size or a particular structure type, so partial
  coverage would materially weaken this task's own point.
- String comparisons must account for the one legitimate,
  already-known source of cross-language difference: Rust `String`s and
  Python `str`s are UTF-8/Unicode by construction, while the underlying
  DMI strings are not guaranteed to be valid UTF-8 (the same open
  question milestone 0009's structured-field-model threat model already
  raised for the C++ side, per the git history referenced in this
  repository's own recent commits) - if `smbios_c`'s own
  `smbios_entry_render()` and typed getters already assume/guarantee
  valid UTF-8 (confirm this by reading `smbios_c/src/smbios_c.cpp`
  before writing this task's implementation), this constraint reduces to
  "no difference expected"; if not, document the exact byte-level
  comparison strategy used instead (e.g. compare raw bytes before each
  language's own string decoding, not the decoded `String`/`str`
  objects) rather than silently normalizing a real discrepancy away.
- On a CI runner exposing no real SMBIOS table, all three vantage points
  must agree on the *same* "no table" result (empty/zero-entry output,
  or the matching `NoTableFound`-equivalent on each binding's open
  call) - this task's own success criterion must hold in that
  degraded case too, not only when real hardware data exists.

## Tests

(This task's own deliverable - `compare_binding_round_trip.py` - is
itself the test; there is no separate meta-test layer above it.)

## Success criteria

- [ ] All three vantage points (C, Rust, Python) produce identical
      output for every entry in a real SMBIOS table on at least one CI
      platform.
- [ ] The known UTF-8/DMI-string discrepancy risk is either confirmed
      absent (and why) or handled with an explicit, documented
      comparison strategy - not silently papered over.
- [ ] The "no real table" degraded case produces matching results across
      all three vantage points.
