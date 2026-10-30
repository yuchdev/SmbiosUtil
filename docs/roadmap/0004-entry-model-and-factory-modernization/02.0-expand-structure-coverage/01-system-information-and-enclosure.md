# 01 - System Information and Enclosure

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `SystemInformationEntry` (`SMBiosHandler::SystemInformation`,
type `1`) and `SystemEnclosureEntry` (`SMBiosHandler::SystemEnclosure`,
type `3`), the two structure types most commonly consumed right after BIOS
Information in a typical `dmidecode`-style report (manufacturer/product
name/serial/UUID, and chassis type/boot-up state/power/thermal state
respectively).

## Files

- **Create** `smbios/include/smbios/system_information_entry.h` / `.cpp`:
  fields per the DMTF spec's "System Information" table - manufacturer,
  product name, version, serial number string indices (`0x04`-`0x07`),
  UUID (`0x08`, 16 bytes - note this is **not** a simple `read_uN`, it
  needs a dedicated UUID-to-string formatter, new in this task), wake-up
  type (`0x18`), SKU number and family string indices (`0x19`-`0x1A`, 2.4+).
- **Create** `smbios/include/smbios/system_enclosure_entry.h` / `.cpp`:
  manufacturer string index, enclosure type, version/serial/asset tag
  string indices, boot-up/power/thermal/security status enums, OEM-defined
  (`0x12`), height/power cords/contained elements (2.1+), SKU number (2.7+).
- **Modify** `smbios_util/src/command_line_parser.cpp` / `main.cpp`: add
  display support for both.

## Functions and Classes

- `SystemInformationEntry` - new, including a `get_uuid_string()` helper
  (byte-to-hex-with-dashes formatting; the one genuinely new formatting
  concern in this task, since nothing in the existing codebase emits a
  UUID today).
- `SystemEnclosureEntry` - new, following the existing
  `bitset_to_properties`/single-enum-lookup patterns already established
  by `BiosInformationEntry`/`MemoryDeviceEntry` for its status enums.

## Constraints

- The SMBIOS UUID field's byte order has a well-known legacy quirk (the
  first three fields are little-endian per the SMBIOS spec's historical
  accommodation of Windows' GUID convention, while the last two are
  big-endian/network order) - get this right against the spec, not by
  assumption; add a dedicated test fixture with a known UUID byte sequence
  and its expected string form.
- Both classes' constructors validate `header.type` against their own
  `SMBiosHandler` value, matching the existing convention
  (`BiosInformationEntry`'s constructor throwing on a type mismatch).

## Tests

- `tests/unit/system_information_entry_test.cpp`,
  `tests/unit/system_enclosure_entry_test.cpp` (new): synthetic buffers at
  each documented length tier; the UUID fixture described above.

## Success criteria

- [ ] Both types register via `REGISTER_SMBIOS_ENTRY` and resolve through
      `SMBiosEntryFactory::create()`.
- [ ] UUID formatting matches the DMTF spec's byte-order convention,
      verified against a known-good fixture.
- [ ] `smbios_util` displays both types on real hardware.
