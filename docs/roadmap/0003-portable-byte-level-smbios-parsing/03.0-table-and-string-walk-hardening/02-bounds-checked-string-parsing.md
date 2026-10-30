# 02 - Bounds-checked string parsing

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

`AbstractSMBiosEntry::parse_dmi_strings()`
([abstract_smbios_entry.cpp:16-30](/smbios/src/abstract_smbios_entry.cpp))
walks `string_section` by repeatedly calling
`std::string(reinterpret_cast<const char*>(string_section))` - which reads
until the next `\0` with no upper bound at all - and its loop condition
`while (string_section[0] != 0 && string_section[1] != 0)` reads two bytes
with no check that either is still inside the table. A structure whose
string section is truncated (no terminating double-null before the mapped
buffer ends) causes an unbounded read.

## Files

- **Modify** `smbios/src/abstract_smbios_entry.cpp`: thread a `table_end`
  (or an explicit remaining-length) parameter into `parse_dmi_strings()` -
  sourced from the same `SMBios::get_table_base() + get_table_size()` the
  table walker already uses - and check it before each `std::string`
  construction and before each double-null probe:
  ```cpp
  void AbstractSMBiosEntry::parse_dmi_strings(const uint8_t* table_end) {
      const uint8_t* string_section = header_->data + header_->length;
      while (string_section < table_end && string_section[0] != 0) {
          const char* start = reinterpret_cast<const char*>(string_section);
          const uint8_t* terminator = std::find(string_section, table_end, 0);
          if (terminator == table_end) break; // unterminated - stop, don't read past the buffer
          dmi_strings_.push_back(std::string(start, reinterpret_cast<const char*>(terminator)));
          string_section = terminator + 1;
      }
  }
  ```
- **Modify** `smbios/include/smbios/abstract_smbios_entry.h`: update
  `parse_dmi_strings()`'s signature and its one call site in the
  constructor (`AbstractSMBiosEntry(const DMIHeader&, const uint8_t*
  table_end)` or equivalent - exact signature change to be finalized
  during implementation, since every entry constructor currently calls the
  base with just `header`).

## Functions and Classes

- `AbstractSMBiosEntry::parse_dmi_strings()` - bounds-checked rewrite.
- `AbstractSMBiosEntry::AbstractSMBiosEntry(const DMIHeader&, ...)` -
  signature gains a table-end bound; every concrete entry constructor
  (`BiosInformationEntry`, `MemoryDeviceEntry`, `PortConnectionEntry`) and
  the factory (`SMBiosEntryFactory::create`) pass it through.

## Constraints

- This changes a constructor signature shared by every entry type - this
  task's blast radius is the whole entry hierarchy, not just
  `AbstractSMBiosEntry` itself. Confirm `SMBiosEntryFactory` (unchanged
  structurally by milestone 0004 until Story 01.0 of that milestone lands)
  still compiles against the new signature before considering this task
  done.
- `std::find` from `<algorithm>` is already an acceptable dependency
  (`smbios.cpp` already includes `<algorithm>`); no new third-party
  dependency.

## Tests

- `tests/unit/dmi_string_parsing_test.cpp` (new): a synthetic structure
  whose string section is deliberately truncated before any double-null -
  assert parsing stops at the buffer boundary with whatever strings were
  fully readable, rather than reading past it.

## Success criteria

- [ ] AddressSanitizer reports no out-of-bounds read on the truncated
      string-section fixture.
- [ ] Existing functional test's DMI strings (vendor, version, release
      date, etc.) are unchanged on real hardware.
