# 02 - Management Device, Component, and Threshold Data

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `ManagementDeviceEntry` (`SMBiosHandler` value `34`), DSP0134
3.9.0 section 7.35; `ManagementDeviceComponentEntry` (value `35`), section
7.36; and `ManagementDeviceThresholdDataEntry` (value `36`), section
7.37.

## Files

- **Create** `libsmbios/include/smbios/tables/management_device_entry.h` /
  `.cpp`:
  - 0x04 Description (STRING)
  - 0x05 Type (BYTE, ENUM - see 7.35.1; name the accessor
    `get_device_type()`, same "Type" name-collision caveat as Story
    04.0's `PointingDeviceEntry`)
  - 0x06 Address (DWORD)
  - 0x0A Address Type (BYTE, ENUM - see 7.35.2)
- **Create**
  `libsmbios/include/smbios/tables/management_device_component_entry.h` /
  `.cpp`:
  - 0x04 Description (STRING)
  - 0x05 Management Device Handle (WORD)
  - 0x07 Component Handle (WORD, references a probe/cooling-device
    structure - 7.27/7.28/7.29/7.30)
  - 0x09 Threshold Handle (WORD, references a Management Device
    Threshold Data structure, or `0xFFFF` if none - confirm the
    "no threshold" sentinel value against the full spec table, since this
    task's own excerpt did not show that detail explicitly)
- **Create**
  `libsmbios/include/smbios/tables/management_device_threshold_data_entry.h` /
  `.cpp`:
  - 0x04 Lower Threshold - Non-critical (WORD; `0x8000` = unavailable)
  - 0x06 Upper Threshold - Non-critical (WORD; `0x8000` = unavailable)
  - 0x08 Lower Threshold - Critical (WORD; `0x8000` = unavailable)
  - 0x0A Upper Threshold - Critical (WORD; `0x8000` = unavailable)
  - 0x0C Lower Threshold - Non-recoverable (WORD; `0x8000` = unavailable)
  - 0x0E Upper Threshold - Non-recoverable (WORD; `0x8000` = unavailable)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all three.

## Functions and Classes

- `ManagementDeviceEntry` - new. `get_description_string()`,
  `get_device_type()` + `get_device_type_string()`, `get_address()`,
  `get_address_type()` + `get_address_type_string()`.
- `ManagementDeviceComponentEntry` - new. `get_description_string()`,
  `get_management_device_handle()`, `get_component_handle()`,
  `get_threshold_handle()` + `has_threshold()` (sentinel check, value
  pending confirmation per the note above).
- `ManagementDeviceThresholdDataEntry` - new. 6 accessor pairs
  (`get_lower_threshold_non_critical()` +
  `is_lower_threshold_non_critical_known()`, and so on for upper
  non-critical, lower/upper critical, lower/upper non-recoverable) - all
  sharing the same `0x8000` unknown-sentinel idiom already used
  repeatedly across this milestone's probe-family types (Story 04.0).
  Note each threshold's *physical unit* (mV, mA, 1/10 degree C, or RPM)
  is determined by whichever probe/cooling-device structure this
  threshold data is attached to via a Management Device Component's own
  Threshold Handle - this class exposes only the raw numeric values with
  no unit label of its own, per the spec's own framing ("as defined by
  the associated probe or cooling-unit component structure").

## Constraints

- Confirm `ManagementDeviceComponentEntry`'s "no threshold" sentinel value
  for Threshold Handle against DSP0134 3.9.0's full Table 110 and its
  surrounding prose before implementing `has_threshold()` - common SMBIOS
  convention for "this optional handle is absent" is `0xFFFF`, but verify
  rather than assume.
- `ManagementDeviceThresholdDataEntry` deliberately carries no unit
  information (see Functions and Classes above) - do not invent a
  `get_unit_string()` or similar; that information does not live in this
  structure per spec.
- Cross-reference consistency between these three types (Management
  Device's own handle, referenced by Component's Management Device
  Handle; Component's own handle, referenced by Threshold Data only
  implicitly via whatever structure holds the Threshold Handle pointing
  at it) is not validated by any of these three classes - matching this
  codebase's existing "expose raw handles, do not resolve them" precedent
  (see this story's own Key constraints note).

## Tests

- `tests/unit/management_device_entry_test.cpp`,
  `tests/unit/management_device_component_entry_test.cpp`,
  `tests/unit/management_device_threshold_data_entry_test.cpp` (new):
  synthetic buffers per documented enum value (Type, Address Type); the
  confirmed "no threshold" sentinel case for Component; all 6
  unknown-sentinel cases for Threshold Data (at least one, not
  necessarily all 6 exhaustively, if the 6 fields are read through one
  shared private helper already covered by Story 04.0's probe tests'
  precedent for that helper's own correctness).

## Success criteria

- [ ] All three types register and resolve through the factory
      (`SMBiosHandler` values 34, 35, 36).
- [ ] Threshold Handle's "no threshold" sentinel confirmed against the
      spec and covered by a test case.
- [ ] `smbios` CLI displays all three on real hardware where present.
