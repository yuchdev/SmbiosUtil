# Story 06.0 - Power, Onboard-Device, and Additional-Info Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement System Power Supply (39, DSP0134 3.9.0 section 7.40),
Additional Information (40, section 7.41), and Onboard Devices Extended
Information (41, section 7.42).

## Tasks

| #  | Document                                                      | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [System power supply](01-system-power-supply.md)                       | ✅ Done        | 04     |
| 02 | [Additional information](02-additional-information.md)                 | ✅ Done        | 04     |
| 03 | [Onboard devices extended information](03-onboard-devices-extended-information.md) | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                  | ⬜ Not started | -      |

Three types, three tasks: System Power Supply (39, flat fixed-offset
structure with three cross-reference handles into the probe/cooling-
device family Story 04.0 already implemented), Additional Information
(40, this milestone's other genuinely variable-length trailing-array
type beyond System Event Log - each entry is itself variable-length,
unlike Group Associations' fixed-3-byte items or Memory Channel's
fixed-3-byte items), and Onboard Devices Extended Information (41, the
direct Type-10-replacement, sharing its bus-address triplet shape with
`SystemSlotsEntry`'s and `PortConnectionEntry`'s existing precedent).
Each is structurally distinct enough from the other two to warrant its
own task rather than being batched.

## Key constraints

- Every new class follows the shape already established: constructor,
  type check, `get_type()`/`render_to_description()`, `read_uN` +
  length-guard accessors.
- System Power Supply (39)'s three handle fields (Input Voltage Probe
  Handle, Cooling Device Handle, Input Current Probe Handle) reference
  structure types this milestone already implements (26, 27, 29, Story
  04.0) - expose the raw handles only, same "no cross-structure
  resolution" precedent as every other handle field in this codebase.
- Additional Information (40)'s per-entry `Value` field (7.41.1, offset
  0x05 of each entry) is explicitly "the same type and size as the field
  being referenced by this Additional Information Entry" - its width is
  **not knowable from this structure alone** (it depends on decoding the
  referenced structure's own field at `Referenced Offset`, which this
  codebase has no generic reflection/field-width lookup for across every
  entry type). **Explicit scope cutoff (R8):** `get_value_bytes_at(size_t
  index)` returns the raw trailing bytes of each entry (computed as
  `entry_length - 5`) rather than attempting to interpret them as a
  specific integer width - document this limitation in the class doc
  comment.
- Onboard Devices Extended Information (41)'s Device Type byte (0x05)
  packs a 1-bit enabled/disabled flag (bit 7) with a 7-bit device-type
  enum (bits 6:0) in the same byte - two independent accessors
  (`is_device_enabled()`, `get_device_type()` masking bit 7 off) rather
  than one that conflates them, matching `MemoryDeviceEntry`'s or
  `SystemSlotsEntry`'s existing precedent for packed-bitfield-plus-enum
  bytes.
- Onboard Devices Extended Information (41)'s Segment Group
  Number/Bus Number/Device-Function Number triplet (0x07-0x0A) is
  byte-for-byte the same shape as `SystemSlotsEntry`'s own segment/bus/
  device-function fields (0x0D-0x10) - reuse that existing class's
  `get_device_number()`/`get_function_number()` bit-splitting logic
  pattern (not the code itself - these are two different classes with no
  shared base beyond `AbstractSMBiosEntry` - but the same bit-shift
  arithmetic, confirmed working there already).

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/system_power_supply_entry.h` /
  `libsmbios/src/system_power_supply_entry.cpp` (new)
- `libsmbios/include/smbios/tables/additional_information_entry.h` /
  `libsmbios/src/additional_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/onboard_devices_extended_information_entry.h` /
  `libsmbios/src/onboard_devices_extended_information_entry.cpp` (new)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all three.
