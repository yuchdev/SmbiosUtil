# 03 - Group Associations

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `GroupAssociationsEntry` (`SMBiosHandler` value `14`), DSP0134
3.9.0 section 7.15.

## Files

- **Create** `libsmbios/include/smbios/tables/group_associations_entry.h` /
  `.cpp`:
  - 0x04 Group Name (BYTE, STRING)
  - 0x05 + `3*i` Item Type (BYTE), for `i` in `0 .. item_count-1`
  - 0x06 + `3*i` Item Handle (WORD), for `i` in `0 .. item_count-1`
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `GroupAssociationsEntry` - new. `get_group_name_string()`,
  `get_item_count()` (computed as `(get_entry_size() - 5) / 3`, per the
  spec's own explicit formula - **not** backed by a structure field),
  `get_item_type_at(size_t index)`, `get_item_handle_at(size_t index)`
  (both 0-based, bounds-checked against `get_item_count()`).

## Constraints

- Per the spec's own note (7.15): "no further fields can be added to this
  structure without adversely affecting existing software's ability to
  properly parse the data" - `Length` *is* the only count signal this
  structure will ever have. `get_item_count()` must not attempt to read a
  count field that does not exist; it is purely
  `(get_entry_size() - 5) / 3`, with a guard returning 0 if
  `get_entry_size() < 5`.
- `get_item_type_at()`/`get_item_handle_at()` read at offsets
  `5 + 3*index` and `6 + 3*index` respectively, each independently guarded
  against `get_entry_size()` (not just against `get_item_count()`) in
  case of a malformed structure whose `Length` does not actually divide
  evenly by 3 after subtracting 5.
- Item Type values are themselves `SMBiosHandler` values (the type of the
  *other* structure being referenced, e.g. `4` for Processor Information,
  `7` for Cache Information, per the spec's own worked example) - no new
  enum needed; reuse `SMBios::SMBiosHandler` for rendering a human-readable
  name where one is registered, falling back to the raw numeric value
  otherwise.

## Tests

- `tests/unit/group_associations_entry_test.cpp` (new): synthetic buffers
  with 0, 1, and 3+ items; a buffer whose `Length` does not divide evenly
  by 3 after subtracting 5 (malformed - verify no out-of-bounds read, per
  this milestone's R2 bounds-checking bar carried over from milestone
  0003/0006).

## Success criteria

- [ ] Registers and resolves through the factory for `SMBiosHandler == 14`.
- [ ] Item count is derived from `Length`, never read as a structure field.
- [ ] Malformed-length case does not read past `get_entry_size()`.
- [ ] `smbios` CLI displays the type on real hardware where present.
