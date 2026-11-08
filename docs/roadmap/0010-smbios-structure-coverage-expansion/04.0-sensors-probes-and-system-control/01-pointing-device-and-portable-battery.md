# 01 - Pointing Device and Portable Battery

**Parent story:** [README.md](README.md)
**Status:** ✅ Done

## Objective

Implement `PointingDeviceEntry` (`SMBiosHandler` value `21`), DSP0134
3.9.0 section 7.22, and `PortableBatteryEntry` (value `22`), section 7.23.

## Files

- **Create** `libsmbios/include/smbios/tables/pointing_device_entry.h` /
  `.cpp`:
  - 0x04 Type (BYTE, ENUM - see 7.22.1; note: same field name as the
    structure header's own `Type` byte at offset 0x00 - name the accessor
    `get_device_type()` to avoid confusion with `get_type()`'s inherited
    "structure kind" meaning)
  - 0x05 Interface (BYTE, ENUM - see 7.22.2)
  - 0x06 Number of Buttons (BYTE)
- **Create** `libsmbios/include/smbios/tables/portable_battery_entry.h` /
  `.cpp`:
  - 0x04 Location (STRING)
  - 0x05 Manufacturer (STRING)
  - 0x06 Manufacture Date (STRING; 0 = "see SBDS Manufacture Date" for
    2.2+ Smart Battery implementations - this class does not decode SBDS
    data itself, only reports whether the string index is 0)
  - 0x07 Serial Number (STRING; same 0-means-"see SBDS" convention)
  - 0x08 Device Name (STRING)
  - 0x09 Device Chemistry (BYTE, ENUM - see 7.23.1; 0x02/Unknown for
    Smart Battery per spec convention)
  - 0x0A Design Capacity (WORD, mWh; 0 = unknown; 2.2+ multiplied by a
    Design Capacity Multiplier - **not present in this structure itself**,
    confirm against the full spec table whether that multiplier is a
    separate field this excerpt omitted, before assuming raw mWh)
  - 0x0C Design Voltage (WORD, mV; 0 = unknown)
  - 0x0E SBDS Version Number (STRING)
  - 0x0F Maximum Error in Battery Data (BYTE, percentage 0-100; 0xFF =
    unknown)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for both.

## Functions and Classes

- `PointingDeviceEntry` - new. `get_device_type()` +
  `get_device_type_string()`, `get_interface()` +
  `get_interface_string()`, `get_button_count()`.
- `PortableBatteryEntry` - new. `get_location_string()`,
  `get_manufacturer_string()`, `get_manufacture_date_string()` (empty
  string + a separate `uses_sbds_manufacture_date()` predicate when the
  index is 0, per the 2.2+ Smart Battery convention),
  `get_serial_number_string()` (same `uses_sbds_serial_number()` pairing),
  `get_device_name_string()`, `get_device_chemistry()` +
  `get_device_chemistry_string()`, `get_design_capacity()` +
  `is_design_capacity_known()`, `get_design_voltage()` +
  `is_design_voltage_known()`, `get_sbds_version_string()`,
  `get_maximum_error_percent()` + `is_maximum_error_known()`.

## Constraints

- Confirm the Design Capacity Multiplier question (noted inline above)
  against DSP0134 3.9.0's full Table 90 directly before finalizing
  `get_design_capacity()` - this task spec's own source excerpt did not
  show a separate multiplier field in the byte-offset table, only prose
  mentioning one; if the full spec table confirms no such field exists in
  this structure (the multiplier may be described only in prose as an
  external multiplier applied by the *reader*, not a field the structure
  itself carries), `get_design_capacity()` returns the raw mWh value
  as-is and that resolution should be noted in the class doc comment so a
  later reader does not reopen the question.
- `PointingDeviceEntry`'s field at offset 0x04 is itself also literally
  named "Type" in the spec table, distinct from the structure header's
  `Type` byte at offset 0x00 (which `AbstractSMBiosEntry`/the factory
  already consume) - name the accessor `get_device_type()`, never
  `get_type()`, to avoid shadowing/confusing the inherited
  `SMBiosInterface::get_type()` (which returns the human-readable
  structure-kind string, e.g. "Built-in Pointing Device", not this
  per-device-type enum).

## Tests

- `tests/unit/pointing_device_entry_test.cpp`,
  `tests/unit/portable_battery_entry_test.cpp` (new): synthetic buffers
  per documented enum value, SBDS-indicator (string index 0) cases for
  Manufacture Date and Serial Number, and the unknown sentinels for
  Design Capacity/Voltage/Maximum Error.

## Success criteria

- [x] Both types register and resolve through the factory
      (`SMBiosHandler` values 21 and 22).
- [x] SBDS-indicator and unknown-sentinel cases are each covered by a
      distinct test case.
- [x] `smbios` CLI displays both types on real hardware where present.
