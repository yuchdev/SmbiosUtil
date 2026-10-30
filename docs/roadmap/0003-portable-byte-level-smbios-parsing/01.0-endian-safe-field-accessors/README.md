# Story 01.0 - Endian-Safe Field Accessors

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Add the `read_u8`/`read_u16`/`read_u32`/`read_u64` little-endian byte
accessors (contract **C1**) and convert `SMBios::read_smbios_table()`'s own
`DMIHeader` extraction - currently `*reinterpret_cast<const
DMIHeader*>(current_structure_begin)` at [smbios.cpp:167](/smbios/src/smbios.cpp) -
to use them for `type`/`length`/`handle`, proving the technique on the one
header type every entry already depends on before Story 02.0 converts the
three concrete entry classes.

## Tasks

| #  | Document                                                      | Status         | Blocks |
|----|--------------------------------------------------------------------|----------------|--------|
| 01 | [Byte reader utility](01-byte-reader-utility.md)                     | ⬜ Not started | 02     |
| 02 | [Replace DMIHeader reinterpret_cast](02-replace-dmiheader-reinterpret-cast.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                 | ⬜ Not started | -      |

## Key constraints

- `read_u8`/`read_u16`/`read_u32`/`read_u64` take a raw `const uint8_t*`
  and an offset, with no bounds checking of their own - bounds-checking is
  the *caller's* responsibility (Story 02.0's length guards, Story 03.0's
  table-walk guards), matching the project's own `c-versions/` dmidecode
  reference, where the low-level `WORD()`/`DWORD()` macros are also
  unchecked and every call site checks `h->length` first.
- No new runtime dependency - plain C++20, header-only if practical.

## Files or directories created or modified by this story

- `smbios/include/smbios/byte_reader.h` - new, header-only accessor
  functions.
- `smbios/src/smbios.cpp` - `read_smbios_table()`'s `DMIHeader` extraction
  converted to use the new accessors for `type`/`length`/`handle`.
