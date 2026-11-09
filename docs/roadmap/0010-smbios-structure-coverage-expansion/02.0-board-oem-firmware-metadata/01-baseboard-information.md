# 01 - Baseboard Information

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `BaseboardInformationEntry` (`SMBiosHandler` value `2`), DSP0134
3.9.0 section 7.3.

## Files

- **Create** `libsmbios/include/smbios/tables/baseboard_information_entry.hpp` /
  `libsmbios/src/baseboard_information_entry.cpp`:
  - 0x04 Manufacturer (STRING)
  - 0x05 Product (STRING)
  - 0x06 Version (STRING)
  - 0x07 Serial Number (STRING)
  - 0x08 Asset Tag (STRING)
  - 0x09 Feature Flags (BYTE, bitmask - see 7.3.1's Table 14)
  - 0x0A Location in Chassis (STRING)
  - 0x0B Chassis Handle (WORD)
  - 0x0D Board Type (BYTE, ENUM - see 7.3.2)
  - 0x0E Number of Contained Object Handles `n` (BYTE)
  - 0x0F Contained Object Handles (`n` WORDs)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `BaseboardInformationEntry` - new. Accessors:
  `get_manufacturer_string()`, `get_product_string()`,
  `get_version_string()`, `get_serial_number_string()`,
  `get_asset_tag_string()`, `get_feature_flags()` +
  `get_feature_flags_string()`, `get_location_in_chassis_string()`,
  `get_chassis_handle()`, `get_board_type()` + `get_board_type_string()`,
  `get_contained_object_handle_count()`,
  `get_contained_object_handle_at(size_t index)` (0-based, bounds-checked
  against the count).

## Constraints

- Feature Flags (7.3.1, Table 14) is a bitmask; per footnote 1 on that
  table, the flags are only meaningful ("present") when `Length >= 0x0E`
  **and** `Number of Contained Object Handles != 0` - `get_feature_flags_string()`
  should reflect that "not present" case distinctly from "present, all
  bits clear" (same spirit as `SystemSlotsEntry::get_characteristics_string()`'s
  own "Unknown" vs "None" distinction - do not conflate the two).
- Board Type (7.3.2) is an 11-entry enum (Unknown, Other, Server Blade,
  Connectivity Switch, System Management Module, Processor Module, I/O
  Module, Memory Module, Daughter Board, Motherboard, Processor+Memory
  Module, Processor+I/O Module, Interconnect Board) - confirm the exact
  value list and count against DSP0134 3.9.0's own 7.3.2 table directly
  (not reproduced in full here).
- Contained Object Handles: `get_contained_object_handle_count()` reads
  the count byte at 0x0E (0 if `get_entry_size() < 0x0F`);
  `get_contained_object_handle_at()` reads the WORD at
  `0x0F + index * 2`, guarded against both the count and
  `get_entry_size()` - same two-guard pattern
  `SystemEnclosureEntry`'s contained-elements accessor already
  established.

## Tests

- `tests/unit/baseboard_information_entry_test.cpp` (new): synthetic
  buffers via `SyntheticTableBuilder`, including a buffer at exactly
  `Length == 0x08` (minimum per spec, string fields present, no feature
  flags/location/chassis/board-type/contained-handles), a buffer with
  feature flags present but `Number of Contained Object Handles == 0`
  (flags "not present" per footnote 1), and a buffer with 2+ contained
  object handles.

## Success criteria

- [x] Registers and resolves through the factory for `SMBiosHandler == 2`.
- [x] Minimum-length (0x08) and full-length buffers both parse correctly.
- [x] Feature-flags "not present" case (footnote 1) is distinct from "all
      bits clear."
- [x] `smbios` CLI displays the type on real hardware where present (in
      practice: `smbios`'s display path is fully generic -
      `SMBiosEntryFactory::create()` + `render_to_description()` in a loop
      over every header, with no per-type branching anywhere in
      `main.cpp`/`command_line_parser.cpp` - so every registered type,
      including this one, is "wired in" by registration alone; see this
      task's own commit message for a pre-existing, out-of-scope caveat
      discovered while verifying this on real hardware).
