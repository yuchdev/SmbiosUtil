# 01 - System Power Supply

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `SystemPowerSupplyEntry` (`SMBiosHandler` value `39`), DSP0134
3.9.0 section 7.40.

## Files

- **Create** `libsmbios/include/smbios/tables/system_power_supply_entry.hpp` /
  `.cpp`:
  - 0x04 Power Unit Group (BYTE; 0 = not redundant)
  - 0x05 Location (STRING)
  - 0x06 Device Name (STRING)
  - 0x07 Manufacturer (STRING)
  - 0x08 Serial Number (STRING)
  - 0x09 Asset Tag Number (STRING)
  - 0x0A Model Part Number (STRING)
  - 0x0B Revision Level (STRING)
  - 0x0C Max Power Capacity (WORD, Watts per the field description text
    - but note the spec's own text flags a units inconsistency: "the
    units specified by DMTF for this field are milliwatts" despite the
    field description saying Watts; `0x8000` = unknown. Confirm which
    unit this project's accessor should document as authoritative against
    the full spec text before writing the Doxygen comment - do not
    silently pick one without noting the spec's own internal
    inconsistency in the comment, since a future reader comparing this
    class's output against another tool's (e.g. dmidecode) needs to know
    which convention was chosen and why)
  - 0x0E Power Supply Characteristics (WORD, bit-field - see 7.40.1)
  - 0x10 Input Voltage Probe Handle (WORD, references Type 26)
  - 0x12 Cooling Device Handle (WORD, references Type 27)
  - 0x14 Input Current Probe Handle (WORD, references Type 29)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `SystemPowerSupplyEntry` - new. `get_power_unit_group()` +
  `is_redundant()`, `get_location_string()`, `get_device_name_string()`,
  `get_manufacturer_string()`, `get_serial_number_string()`,
  `get_asset_tag_string()`, `get_model_part_number_string()`,
  `get_revision_level_string()`, `get_max_power_capacity()` +
  `is_max_power_capacity_known()` (document the chosen unit convention -
  see Files above - in this accessor's own Doxygen comment, not just the
  class comment, since it is the one field affected), `get_characteristics()`
  + decoded accessors per 7.40.1 (likely at least a DMM/active PFC/hot-
  replaceable/plugged-in/present set of bits and a "Power Supply Type"
  sub-field - confirm the exact bit layout against the full spec table
  before implementing), `get_input_voltage_probe_handle()` +
  `has_input_voltage_probe()` (sentinel check, value pending
  confirmation), `get_cooling_device_handle()` + `has_cooling_device()`,
  `get_input_current_probe_handle()` + `has_input_current_probe()`.

## Constraints

- Power Supply Characteristics' exact bit layout (7.40.1, Table 117) must
  be confirmed against the full spec table before implementation - this
  task spec's own source excerpt did not include that table's rows.
- The three handle fields' "absent" sentinel (likely `0xFFFF`, matching
  common SMBIOS convention, but confirm against the full table) gates
  each `has_*()` predicate - do not assume `0` means absent for a handle
  field (handle `0` is a theoretically valid, if unusual, handle value
  in SMBIOS; `0xFFFF` is the conventional "no such reference" sentinel
  used elsewhere in the spec).

## Tests

- `tests/unit/system_power_supply_entry_test.cpp` (new): a minimum-length
  buffer (`Length == 0x10`), a buffer with all three optional handles
  present, a buffer with all three absent (sentinel), and the Max Power
  Capacity unknown-sentinel case.

## Success criteria

- [ ] Power Supply Characteristics bit layout confirmed against the full
      spec table.
- [ ] Handle "absent" sentinel confirmed against the full spec table.
- [ ] Registers and resolves through the factory for `SMBiosHandler ==
      39`.
- [ ] `smbios` CLI displays the type on real hardware where present.
