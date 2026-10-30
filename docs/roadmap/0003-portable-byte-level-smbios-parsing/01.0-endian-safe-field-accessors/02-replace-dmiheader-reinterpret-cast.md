# 02 - Replace DMIHeader reinterpret_cast

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (`byte_reader.h`)

## Objective

`SMBios::read_smbios_table()` currently does
`DMIHeader header = *reinterpret_cast<const DMIHeader*>(current_structure_begin);`
([smbios.cpp:167](/smbios/src/smbios.cpp)) to pull `type` (1 byte),
`length` (1 byte) and `handle` (2 bytes) out of the raw table. Replace this
with explicit field reads via `read_u8`/`read_u16`, proving the pattern on
the header every downstream entry type already relies on.

## Files

- **Modify** `smbios/src/smbios.cpp`:
  ```cpp
  DMIHeader header;
  header.type   = read_u8(current_structure_begin, 0);
  header.length = read_u8(current_structure_begin, 1);
  header.handle = read_u16(current_structure_begin, 2);
  header.data   = current_structure_begin;
  ```
  replacing the single-line `reinterpret_cast` assignment.
- **Modify** `smbios/include/smbios/smbios.h`: `DMIHeader` itself can keep
  its current field layout (it is no longer overlaid onto raw memory by
  this code path) - no change needed unless `count_smbios_structures()`
  (Story 03.0) also needs an update, tracked separately there.

## Functions and Classes

- `SMBios::read_smbios_table()` - modified call site only; no signature
  change.

## Constraints

- `header.data` keeps pointing at the raw buffer unchanged - entry classes
  in Story 02.0 read their own fields from `header.data` via the same
  accessors, not from any new copy.
- `#pragma pack(push, 1)` around `DMIHeader` in `smbios.h` may be removed
  once nothing in the codebase overlays it onto raw memory - confirm this
  is true (Story 02.0 and 03.0 must also stop doing so) before removing the
  pragma; if any caller still relies on it, leave it and note why in
  `status.md`.

## Success criteria

- [ ] No `reinterpret_cast<const DMIHeader*>` remains in `smbios.cpp`.
- [ ] Existing functional tests produce byte-identical `type`/`length`/
      `handle` values before and after this change, verified on real
      little-endian hardware.
