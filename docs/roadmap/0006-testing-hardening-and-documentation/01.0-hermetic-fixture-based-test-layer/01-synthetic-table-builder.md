# 01 - Synthetic table builder

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement a small, fluent builder that assembles a well-formed (by
default) in-memory SMBIOS structure table byte-for-byte - header
(`type`/`length`/`handle`), formatted area, string section, double-null
terminator, repeated per structure, ending in an `EndOfTable` marker -
satisfying contract R1.

## Files

- **Create** `smbios/test/unit/synthetic_table_builder.h` / `.cpp`:
  ```cpp
  class SyntheticTableBuilder {
  public:
      SyntheticTableBuilder& add_structure(uint8_t type, std::vector<uint8_t> formatted_area,
                                            std::vector<std::string> strings = {});
      SyntheticTableBuilder& add_raw_bytes(std::vector<uint8_t> raw); // for malformed-table tests (Story 02.0)
      std::vector<uint8_t> build() const; // appends EndOfTable automatically unless add_raw_bytes bypassed it
  };
  ```
  `add_structure` computes `length` from the formatted area's size plus
  the 4-byte header automatically, writes each string followed by `\0`,
  and appends the terminating `\0\0` - matching exactly what
  `AbstractSMBiosEntry::parse_dmi_strings()` (milestone 0003, hardened)
  expects to walk.

## Functions and Classes

- `smbios::test::SyntheticTableBuilder` (or similar namespace - decide
  during implementation whether this lives in `smbios::test` or a
  dedicated `smbios_test_support` namespace to make clear it is test-only).

## Constraints

- `SMBios`'s constructor (post-milestone-0005) always walks the
  `ISmbiosSource` chain - there is no existing entry point that takes a
  raw buffer directly. This task must add one: a test-only constructor
  overload (or a free function `read_headers_from_buffer(const
  std::vector<uint8_t>&) -> std::vector<DMIHeader>` factored out of
  `read_smbios_table()`'s existing logic) that the hermetic tests call
  instead of `SMBios`'s normal constructor. Factoring the table-walk logic
  out from the source-selection logic this cleanly is itself a sign the
  two were already somewhat conflated; keep the change minimal and
  test-only-facing rather than restructuring `SMBios`'s public API.
- This is test-support code (`smbios/test/unit/`), not shipped as part of
  the `smbios` library target - it must not be linked into
  `smbios`/`smbios_util`'s production binaries.
- `add_raw_bytes` exists specifically so Story 02.0's malformed-table
  tests can construct deliberately-broken input (a structure whose
  `length` overruns the buffer, a missing double-null) without the
  builder's own well-formed-by-default logic getting in the way.

## Tests

- `tests/unit/synthetic_table_builder_test.cpp` (new, tests the test
  helper itself): build a two-structure table, feed it through
  `SMBios::read_smbios_table()`, confirm both structures are found with
  the expected `type`/`length`/`handle`/strings.

## Success criteria

- [ ] A table built via `SyntheticTableBuilder` and fed into `SMBios`
      produces the expected header list and DMI strings.
- [ ] `add_raw_bytes` allows constructing a table that violates the
      well-formed assumptions, for Story 02.0's use.
