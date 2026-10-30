# Story 03.0 - Table and String Walk Hardening

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`byte_reader.h`, for the already-converted `DMIHeader` extraction this story builds bounds checks around)

## Scope

Add the bounds checks contract **C3** describes to
`SMBios::read_smbios_table()`/`count_smbios_structures()`
([smbios.cpp:155-229](/smbios/src/smbios.cpp)) and to
`AbstractSMBiosEntry::parse_dmi_strings()`
([abstract_smbios_entry.cpp:16-30](/smbios/src/abstract_smbios_entry.cpp)),
so a truncated or corrupted SMBIOS table degrades to a clear stop condition
instead of reading past the mapped buffer.

## Tasks

| #  | Document                                                      | Status         | Blocks |
|----|--------------------------------------------------------------------|----------------|--------|
| 01 | [Bounds-checked table walk](01-bounds-checked-table-walk.md)         | ⬜ Not started | 03     |
| 02 | [Bounds-checked string parsing](02-bounds-checked-string-parsing.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                | ⬜ Not started | -      |

## Key constraints

- The fix is "stop cleanly at `table_end`," not "throw an exception for
  every malformed table" - this project's existing convention (see
  `read_smbios_table()`'s existing `if (header.length < 4) break;`) is to
  break out of the parse loop and keep whatever structures were already
  parsed; this story extends that same convention to the newly-identified
  gaps, it does not introduce a new error-handling philosophy.
- No change to any already-passing functional test's output (R8) - a
  table this project can already parse correctly must still parse
  identically; only genuinely out-of-bounds reads are newly prevented.

## Files or directories created or modified by this story

- `smbios/src/smbios.cpp` - `read_smbios_table()`,
  `count_smbios_structures()`.
- `smbios/src/abstract_smbios_entry.cpp` - `parse_dmi_strings()`.
