# 03 - Onboard Devices Extended Information

**Parent story:** [README.md](README.md)
**Status:** ✅ Done

## Objective

Implement `OnboardDevicesExtendedInformationEntry` (`SMBiosHandler` value
`41`), DSP0134 3.9.0 section 7.42 - the replacement for the obsolete Type
10 this milestone deliberately excludes (plan.md's Non-goals).

## Files

- **Create**
  `libsmbios/include/smbios/tables/onboard_devices_extended_information_entry.hpp` /
  `.cpp`:
  - 0x04 Reference Designation (STRING - typically the silkscreen label,
    7.42.1)
  - 0x05 Device Type (BYTE: bit 7 = enabled/disabled, bits 6:0 = device
    type enum - see 7.42.2)
  - 0x06 Device Type Instance (BYTE - see 7.42.3)
  - 0x07 Segment Group Number (WORD - see 7.42.4)
  - 0x09 Bus Number (BYTE - see 7.42.4)
  - 0x0A Device/Function Number (BYTE: bits 7:3 = device number, bits
    2:0 = function number - see 7.42.4)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `OnboardDevicesExtendedInformationEntry` - new.
  `get_reference_designation_string()`, `is_device_enabled()` (bit 7 of
  Device Type), `get_device_type()` (bits 6:0, masked) +
  `get_device_type_string()` (7.42.2's enum table), `get_device_type_instance()`,
  `get_segment_group_number()`, `get_bus_number()`,
  `get_device_function_number()`, `get_device_number()` (bits 7:3),
  `get_function_number()` (bits 2:0), and a combined
  `get_bus_address_string()` rendering "Segment:Bus:Device.Function" - the
  exact same rendering shape `SystemSlotsEntry::get_bus_address_string()`
  already produces, including that class's own all-FF/all-FFFF "not
  applicable" sentinel handling if 7.42.4 documents the same sentinel
  convention (confirm against the spec; do not assume without checking,
  even though the field shapes match).

## Constraints

- Device Type's bit-7/bits-6:0 split must be masked explicitly
  (`device_type_byte & 0x7F` for the type, `device_type_byte & 0x80` for
  enabled/disabled) - two independent accessors, not a combined one, per
  this story's own Key constraints note.
- Confirm 7.42.4's sentinel convention for "not applicable" bus-address
  fields (all-FF/all-FFFF, matching `SystemSlotsEntry`'s own 7.10.8, or a
  different convention) before implementing `get_bus_address_string()`'s
  "empty when not applicable" branch - do not copy `SystemSlotsEntry`'s
  exact sentinel check without first checking 7.42.4 independently
  confirms the same values.

## Tests

- `tests/unit/onboard_devices_extended_information_entry_test.cpp` (new):
  synthetic buffers with the device enabled and disabled, each
  documented device-type enum value exercised at least once, and (if
  confirmed per the Constraints note) the "not applicable" bus-address
  sentinel case.

## Success criteria

- [x] Registers and resolves through the factory for `SMBiosHandler ==
      41`.
- [x] Enabled/disabled bit and device-type bits are verified independently
      (a test where one is set and the other varies, to catch an
      accidental shared-mask bug).
- [x] `smbios` CLI displays the type on real hardware where present.

## Research notes (resolved during implementation)

- 7.42.4's "not applicable" bus-address sentinel convention was confirmed
  to match `SystemSlotsEntry`'s own 7.10.8 **exactly** (near-verbatim
  spec text, "devices" substituted for "slots"): all-FF/all-FFFF across
  Segment Group Number (0xFFFF)/Bus Number (0xFF)/Device-Function Number
  (0xFF) simultaneously. `get_bus_address_string()` therefore reuses that
  class's rendering shape and sentinel check, confirmed independently per
  the task's own instruction rather than copied blindly.
- The Device Type enum (7.42.2, Table 121) was confirmed as 16 values,
  01h-10h: Other, Unknown, Video, SCSI Controller, Ethernet, Token Ring,
  Sound, PATA Controller, SATA Controller, SAS Controller, Wireless LAN,
  Bluetooth, WWAN, eMMC (embedded Multi-Media Controller), NVMe
  Controller, UFS Controller.
