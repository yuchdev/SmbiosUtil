# 03 - Voltage, Cooling, Temperature, and Electrical Current Probes

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `VoltageProbeEntry` (`SMBiosHandler` value `26`), DSP0134 3.9.0
section 7.27; `CoolingDeviceEntry` (value `27`), section 7.28;
`TemperatureProbeEntry` (value `28`), section 7.29; and
`ElectricalCurrentProbeEntry` (value `29`), section 7.30.

## Files

- **Create** `libsmbios/include/smbios/tables/voltage_probe_entry.hpp` /
  `.cpp`:
  - 0x04 Description (STRING)
  - 0x05 Location and Status (BYTE, bit-field - see 7.27.1)
  - 0x06 Maximum Value (WORD, mV; `0x8000` = unknown)
  - 0x08 Minimum Value (WORD, mV; `0x8000` = unknown)
  - 0x0A Resolution (WORD, tenths of mV; `0x8000` = unknown)
  - 0x0C Tolerance (WORD, +/- mV; `0x8000` = unknown)
  - 0x0E Accuracy (WORD, +/- 1/100 %; `0x8000` = unknown)
  - 0x10 OEM-defined (DWORD)
- **Create** `libsmbios/include/smbios/tables/cooling_device_entry.hpp` /
  `.cpp`:
  - 0x04 Temperature Probe Handle (WORD)
  - 0x06 Device Type and Status (BYTE, bit-field - see 7.28.1)
  - 0x07 Cooling Unit Group (BYTE; 0 = not redundant)
  - 0x08 OEM-defined (DWORD)
  - 0x0C Nominal Speed (WORD, rpm; `0x8000` = unknown/non-rotating;
    **present only if `Length > 0x0C`**)
  - 0x0E Description (STRING; **present only if `Length >= 0x0F`**)
- **Create** `libsmbios/include/smbios/tables/temperature_probe_entry.hpp` /
  `.cpp`: identical shape to Voltage Probe at the same offsets, units in
  1/10 degree C instead of mV (Maximum/Minimum Value, Resolution in
  1/1000 degree C, Tolerance in 1/10 degree C, Accuracy in 1/100 %).
- **Create**
  `libsmbios/include/smbios/tables/electrical_current_probe_entry.hpp` /
  `.cpp`: identical shape to Voltage/Temperature Probe at the same
  offsets through 0x10 (units in milliamps), **plus** 0x14 Nominal Value
  (WORD, mA; `0x8000` = unknown; **present only if `Length > 0x14`**).
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all four.

## Functions and Classes

- `VoltageProbeEntry`, `TemperatureProbeEntry`,
  `ElectricalCurrentProbeEntry` - new, each with: `get_description_string()`,
  `get_location_and_status()` + decoded location/status accessors (7.27.1
  for Voltage - confirm whether Temperature/Electrical Current share the
  *identical* bit layout or each has its own numbered subsection
  [7.29.1/7.30.1 both exist per the TOC] before assuming a shared decode
  function; likely similar but not guaranteed byte-identical),
  `get_maximum_value()` + `is_maximum_value_known()`,
  `get_minimum_value()` + `is_minimum_value_known()`,
  `get_resolution()` + `is_resolution_known()`, `get_tolerance()` +
  `is_tolerance_known()`, `get_accuracy()` + `is_accuracy_known()`,
  `get_oem_defined()`. `ElectricalCurrentProbeEntry` additionally exposes
  `get_nominal_value()` + `is_nominal_value_known()` +
  `has_nominal_value()` (the length-gate predicate).
- `CoolingDeviceEntry` - new. `get_temperature_probe_handle()`,
  `get_device_type_and_status()` + decoded accessors (7.28.1),
  `get_cooling_unit_group()` + `is_redundant()` (group != 0),
  `get_oem_defined()`, `get_nominal_speed()` + `is_nominal_speed_known()`
  + `has_nominal_speed()` (length-gate: `Length > 0x0C`),
  `get_description_string()` + `has_description()` (length-gate:
  `Length >= 0x0F`).

## Constraints

- Shared "is this WORD the `0x8000` unknown sentinel" check: implement
  once per `.cpp` (a private static helper, or a free function in an
  internal anonymous namespace) rather than inlining the comparison 5
  times per class x 4 classes = 20 call sites with no shared helper, per
  this story's own Key constraints note.
- Do not assume Voltage/Temperature/Electrical Current Probe's
  Location-and-Status bit layouts are byte-identical across all three
  without checking each one's own numbered subsection (7.27.1, 7.29.1,
  7.30.1) - the physical-location bits are likely shared, but the
  "status" bits may legitimately differ per probe kind (e.g. a
  voltage-specific "under voltage" vs a temperature-specific
  "non-critical/critical/non-recoverable" distinction with different
  threshold semantics); confirm all three subsections individually
  before writing one shared decode table.
- `CoolingDeviceEntry`'s two trailing fields each have their own,
  independent length gate (`> 0x0C` for Nominal Speed, `>= 0x0F` for
  Description) - a buffer with `Length == 0x0D` or `0x0E` has Nominal
  Speed but not Description; test both intermediate lengths, not just
  the two endpoints.

## Tests

- `tests/unit/voltage_probe_entry_test.cpp`,
  `tests/unit/cooling_device_entry_test.cpp`,
  `tests/unit/temperature_probe_entry_test.cpp`,
  `tests/unit/electrical_current_probe_entry_test.cpp` (new): synthetic
  buffers at each documented minimum length, unknown-sentinel cases for
  every WORD field, and - for Cooling Device specifically - all three
  length tiers (`0x0C`, `0x0D`-`0x0E`, `>= 0x0F`) distinguishing Nominal
  Speed's and Description's independent presence.

## Success criteria

- [ ] All four types register and resolve through the factory
      (`SMBiosHandler` values 26, 27, 28, 29).
- [ ] Unknown-sentinel cases covered for every WORD field across all four
      classes.
- [ ] Cooling Device's two independent length gates each covered by a
      distinct test case at an intermediate length.
- [ ] `smbios` CLI displays all four types on real hardware where present.
