# Story 02.0 - Board, OEM, and Firmware Metadata Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** 🔶 In progress (3/4 tasks)
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement 5 of the smallest and earliest-numbered remaining structure
types: Baseboard (or Module) Information (2), OEM Strings (11), System
Configuration Options (12), Firmware Language Information (13), and Group
Associations (14) - DSP0134 3.9.0 sections 7.3, 7.12, 7.13, 7.14, 7.15.
Every one of these is written directly against
`include/smbios/tables/` (Story 01.0's new location) and self-registered
via `REGISTER_SMBIOS_ENTRY`.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Baseboard information](01-baseboard-information.md)                   | ✅ Complete | 04     |
| 02 | [OEM strings, config options, firmware language](02-oem-strings-config-options-firmware-language.md) | ✅ Complete | 04     |
| 03 | [Group associations](03-group-associations.md)                         | ✅ Complete | 04     |
| 04 | [Tests](04-tests.md)                                                  | ⬜ Not started | -      |

Task 01 is split out alone because Baseboard Information (2) is the only
one of the 5 with a flags enum and a contained-object-handles trailing
array (same "handle array after the fixed fields" shape as
`SystemEnclosureEntry`'s contained-elements array). Task 02 groups OEM
Strings (11), System Configuration Options (12), and Firmware Language
Information (13) because the first two are byte-for-byte identical in
shape (a single `Count` byte, structure body otherwise empty - the actual
content is 100% in the trailing DMI string list) and the third, while
slightly larger (a flags byte and 15 reserved bytes), is still a flat
fixed-offset structure with no trailing array. Task 03 is Group
Associations (14) alone because its trailing repeated-triplet array (3
bytes per item: 1-byte item type + 2-byte item handle, count implied by
`(Length - 5) / 3` rather than an explicit count field) is a distinct
parsing shape from every other type in this story.

## Key constraints

- Every new class follows the shape `SystemSlotsEntry` already
  demonstrates post milestone 0003/0004: constructor
  `(const DMIHeader&, const SMBiosVersion&, const uint8_t*)`, type check
  against its own `SMBiosHandler` value, `get_type()`/
  `render_to_description()` overrides, field getters via
  `read_uN(raw_data(), offset)` behind a `get_entry_size() >= offset +
  width` guard.
- OEM Strings (11) and System Configuration Options (12) have **no
  fixed-offset fields beyond `Count` at 0x04** - their entire informational
  content is the trailing DMI string list (`Count` strings, indices 1..N).
  Both classes expose `get_string_count()` plus a
  `get_string_at(size_t index)` (1-based, matching every other
  `dmi_string()` call site's own 1-based convention) rather than a single
  flat accessor.
- Group Associations (14)'s item count is **not** a structure field - it
  is derived from `(get_entry_size() - 5) / 3`, per the spec's own
  explicit formula (7.15's table). `get_item_count()` computes this; there
  is no `read_uN` call backing it directly.
- Baseboard Information (2)'s `Number of Contained Object Handles` (offset
  0x0E) and the trailing WORD array it introduces follow the exact same
  "count byte, then N WORDs starting right after it" shape
  `SystemEnclosureEntry`'s contained-elements array already established
  in this codebase (milestone 0004 Task 01) - reuse that precedent's
  pattern, not a new one.
- Baseboard's feature-flags byte (offset 0x09, see 7.3.1's Table 14) needs
  its own bit-to-string map, same `bitset_to_properties<T>()` helper every
  existing bitmask-rendering entry class already uses.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/baseboard_information_entry.h` /
  `libsmbios/src/baseboard_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/oem_strings_entry.h` /
  `libsmbios/src/oem_strings_entry.cpp` (new)
- `libsmbios/include/smbios/tables/system_configuration_options_entry.h` /
  `libsmbios/src/system_configuration_options_entry.cpp` (new)
- `libsmbios/include/smbios/tables/firmware_language_information_entry.h` /
  `libsmbios/src/firmware_language_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/group_associations_entry.h` /
  `libsmbios/src/group_associations_entry.cpp` (new)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all 5 (R7).
