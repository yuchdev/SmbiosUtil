# 03 - Firmware Inventory Information, String Property, and Inactive

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `FirmwareInventoryInformationEntry` (`SMBiosHandler` value
`45`), DSP0134 3.9.0 section 7.46; `StringPropertyEntry` (value `46`),
section 7.47; and `InactiveEntry` (value `126`), section 7.48 - the last
type this milestone adds, tacked onto this task per plan.md's own
explicit Non-goals decision rather than given a dedicated task.

## Files

- **Create**
  `libsmbios/include/smbios/tables/firmware_inventory_information_entry.h` /
  `.cpp`:
  - 0x04 Firmware Component Name (STRING)
  - 0x05 Firmware Version (STRING)
  - 0x06 Version Format (BYTE - see 7.46.1)
  - 0x07 Firmware ID (STRING)
  - 0x08 Firmware ID Format (BYTE - see 7.46.2)
  - 0x09 Release Date (STRING, DSP0266 date-time format)
  - 0x0A Manufacturer (STRING)
  - 0x0B Lowest Supported Firmware Version (STRING)
  - 0x0C Image Size (QWORD; `0xFFFFFFFFFFFFFFFF` = unknown)
  - 0x14 Characteristics (WORD, bit-field - see 7.46.3)
  - 0x16 State (BYTE - see 7.46.4)
  - 0x17 Number of Associated Components `n` (BYTE - explicit count
    field, **not** derivable from `Length` alone per the spec's own
    warning, same pattern as Memory Channel/Additional Information)
  - 0x18.. Associated Component Handles (`n` WORDs - this task's own
    source excerpt was cut off exactly at this point; confirm the exact
    starting offset and any intervening field against the full Table 133
    before implementing, since the excerpt's next row was omitted)
- **Create** `libsmbios/include/smbios/tables/string_property_entry.h` /
  `.cpp`:
  - 0x04 String Property ID (WORD - see 7.47.1)
  - 0x06 String Property Value (STRING)
  - 0x07 Parent Handle (WORD)
- **Create** `libsmbios/include/smbios/tables/inactive_entry.h` / `.cpp`:
  no decoded fields - constructor performs the usual type check,
  `get_type()` returns "Inactive," `render_to_description()` states the
  structure should be bypassed by upper-level software (7.48's own
  guidance).
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all three (Inactive's CLI line is one sentence, matching
  Boot Integrity Services' own minimal treatment from Story 05.0).

## Functions and Classes

- `FirmwareInventoryInformationEntry` - new.
  `get_component_name_string()`, `get_version_string()`,
  `get_version_format()` + `get_version_format_string()` (7.46.1),
  `get_firmware_id_string()`, `get_firmware_id_format()` +
  `get_firmware_id_format_string()` (7.46.2), `get_release_date_string()`,
  `get_manufacturer_string()`, `get_lowest_supported_version_string()`,
  `get_image_size()` + `is_image_size_known()`, `get_characteristics()` +
  decoded accessors (7.46.3), `get_state()` + `get_state_string()`
  (7.46.4), `get_associated_component_count()` (explicit field, not
  derived from `Length`), `get_associated_component_handle_at(size_t
  index)`.
- `StringPropertyEntry` - new. `get_property_id()` +
  `get_property_id_string()` (7.47.1's ID table: 0 reserved, 1 = UEFI
  device path, 2-32767 DMTF-reserved, 32768-49151 firmware-vendor-
  reserved, 49152-65535 OEM-reserved - render the reserved ranges as
  "Reserved (DMTF)"/"Reserved (Firmware Vendor)"/"Reserved (OEM)" rather
  than a flat "Unknown," since the ranges themselves are meaningful even
  when no specific ID within them is assigned), `get_value_string()`,
  `get_parent_handle()`.
- `InactiveEntry` - new, minimal (see Files above).

## Constraints

- Firmware Inventory Information's exact layout from offset 0x18 onward
  (where the Associated Component Handles array begins, and whether any
  field sits between 0x17's count byte and the array itself) must be
  confirmed against the complete DSP0134 3.9.0 Table 133 before
  implementation - this task's own source excerpt was truncated at
  exactly that boundary.
- `get_associated_component_count()` must read the explicit field at
  0x17, never derive it from `Length` - third confirmed instance of this
  exact idiom in this milestone (after Memory Channel, Story 05.0, and
  Additional Information's entry *count* field, Story 06.0 - though
  Additional Information's per-entry *length* is still variable, unlike
  this fixed-2-bytes-per-handle array).
- `StringPropertyEntry::get_property_id_string()`'s reserved-range
  rendering (see Functions and Classes) should be covered by a test case
  per range, not just the one assigned value (`1`, UEFI device path).

## Tests

- `tests/unit/firmware_inventory_information_entry_test.cpp`,
  `tests/unit/string_property_entry_test.cpp`,
  `tests/unit/inactive_entry_test.cpp` (new): Firmware Inventory with 0
  and 2+ associated component handles, the Image Size unknown sentinel;
  String Property with the assigned ID (1) and at least one value from
  each reserved range; Inactive with a single minimal-buffer
  registration/resolution case.

## Success criteria

- [x] Associated Component Handles array offset confirmed against the
      full spec table. Table 133 is split across two source pages; the
      0x17 (Number of Associated Components) row is the last row on the
      first page, and the very next row is 0x18 (Associated Component
      Handles, n WORDs) - nothing sits between them.
- [x] All three types register and resolve through the factory
      (`SMBiosHandler` values 45, 46, 126).
- [x] String Property's reserved-range rendering covered by a test case
      per range.
- [x] `smbios` CLI displays all three on real hardware where present (or,
      for Inactive, in a synthetic demonstration if no real machine
      happens to report one) - via the existing generic factory-driven
      `render_to_description()` loop in `smbios/src/main.cpp`; no CLI edit
      needed, same finding as Tasks 01/02.
