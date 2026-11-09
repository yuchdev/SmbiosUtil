# 01 - System Event Log

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-08)

## Objective

Implement `SystemEventLogEntry` (`SMBiosHandler` value `15`), DSP0134
3.9.0 section 7.16, through its 2.1+ supported-log-type-descriptor list
(7.16.1) - not through the event log's own record contents (see scope
cutoff below).

## Files

- **Create** `libsmbios/include/smbios/tables/system_event_log_entry.hpp` /
  `.cpp`:
  - 0x04 Log Area Length (WORD)
  - 0x06 Log Header Start Offset (WORD)
  - 0x08 Log Data Start Offset (WORD)
  - 0x0A Access Method (BYTE) - present per the spec table but omitted
    from this task spec's own excerpt; confirm its exact offset/enum
    against DSP0134 3.9.0's full Table 60 directly before implementing
    (the excerpt used while writing this task had a redacted row at this
    position)
  - 0x0B Log Status (BYTE, bitmask: bit 0 valid, bit 1 full)
  - 0x0C Log Change Token (DWORD)
  - 0x10 Access Method Address (DWORD)
  - 0x14 Log Header Format (BYTE, ENUM, 2.1+)
  - 0x15 Number of Supported Log Type Descriptors `x` (BYTE, 2.1+)
  - 0x16 Length of each Log Type Descriptor `y` (BYTE, 2.1+, currently
    always 2)
  - 0x17..0x17+(x*y)-1 List of Supported Event Log Type Descriptors
    (2.1+)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `SystemEventLogEntry` - new. `get_log_area_length()`,
  `get_log_header_start_offset()`, `get_log_data_start_offset()`,
  `get_access_method()` + `get_access_method_string()`,
  `get_log_status()` + `is_log_area_valid()` + `is_log_area_full()`,
  `get_log_change_token()`, `get_access_method_address()`,
  `get_log_header_format()` (2.1+), `get_supported_descriptor_count()`
  (2.1+), `get_descriptor_length()` (2.1+, expected value `2` but read,
  not assumed - do not hardcode `2` into the offset arithmetic below it),
  `get_log_type_descriptor_at(size_t index)` (2.1+, returns the raw
  2-byte - or `get_descriptor_length()`-byte, if that is ever not 2 -
  descriptor entry at the computed offset).

## Constraints

- **Explicit scope cutoff (R8), document in the class doc comment**: this
  class decodes the System Event Log *structure* (its header, status, and
  supported-type-descriptor list) only. It does not read, access, or
  decode the actual non-volatile event-log records the structure points
  to via Access Method Address (7.16.4-7.16.6: Log Header format,
  7.16.5-6 Log Record format) - doing so would require indexed-I/O,
  memory-mapped, or GPNV-handle access this codebase has no existing OS
  abstraction for (distinct from, and lower-level than, the
  `ISmbiosSource` chain milestone 0005 built, which reads the *SMBIOS
  table itself*, not arbitrary NVRAM the table merely references).
- 2.1+ fields (0x14 onward) must be guarded by both `SMBiosVersion >=
  {2,1}` and `get_entry_size()` - version 2.0 implementations report
  `Length == 0x14` and stop before any of these fields exist.
- The redacted table row noted above (0x0A Access Method) must be
  confirmed against the spec file directly (grep `7.16` and the
  surrounding table in `docs/standard/DSP0134_3.9.0.md`) before writing
  code - do not guess its offset from adjacent fields alone, since this
  task spec's own source excerpt had that one row's content omitted by
  the extraction tool that produced the markdown.

## Tests

- `tests/unit/system_event_log_entry_test.cpp` (new): a version-2.0-shaped
  buffer (`Length == 0x14`, no descriptor list), a version-2.1+ buffer
  with 0 descriptors (`x == 0`, confirming the list-start offset 0x17 is
  correctly treated as absent per the spec's own "If the value is 0, the
  list that starts at offset 17h is not present" note), and a 2.1+ buffer
  with 2+ descriptors.

## Success criteria

- [x] Registers and resolves through the factory for `SMBiosHandler == 15`.
- [x] 2.0-shaped and 2.1+-shaped buffers both parse correctly, including
      the `x == 0` "list not present" case.
- [x] Scope cutoff (event-log *contents* out of scope) is documented in
      the class doc comment.
- [x] `smbios` CLI displays the type on real hardware where present (via
      the generic `SMBiosEntryFactory`/`render_to_description()` path, same
      mechanism every other type uses - still subject to the separately
      tracked CLI-linking bug `task_dc27f15d`, not fixed by this task).
