# Story 04.0 - Sensors, Probes, and Small System-Control Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement Built-in Pointing Device (21, DSP0134 3.9.0 section 7.22),
Portable Battery (22, section 7.23), System Reset (23, section 7.24),
Hardware Security (24, section 7.25), System Power Controls (25, section
7.26), Voltage Probe (26, section 7.27), Cooling Device (27, section
7.28), Temperature Probe (28, section 7.29), and Electrical Current Probe
(29, section 7.30) - 9 types, the largest grouping in this milestone, but
justified by how tightly the last 4 (26-29) share one field shape.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Pointing device and portable battery](01-pointing-device-and-portable-battery.md) | ⬜ Not started | 04     |
| 02 | [Reset, hardware security, power controls](02-reset-security-power-controls.md) | ⬜ Not started | 04     |
| 03 | [Voltage, cooling, temperature, and current probes](03-probes.md) | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                  | ⬜ Not started | -      |

Task 01 groups Built-in Pointing Device (21) and Portable Battery (22) as
the two "describes one piece of attached/auxiliary hardware" types with
otherwise unrelated field shapes. Task 02 groups System Reset (23),
Hardware Security (24), and System Power Controls (25) - three small,
fixed-size, no-string, no-trailing-array structures, all introduced in
spec version 2.2, all describing system-wide control/security settings
rather than a specific physical device. Task 03 batches Voltage Probe
(26), Cooling Device (27), Temperature Probe (28), and Electrical Current
Probe (29) together deliberately: three of the four (26, 28, 29) share an
**identical** field layout (Description, Location and Status,
Maximum/Minimum Value, Resolution, Tolerance, Accuracy, OEM-defined, with
29 adding a 2.2+/length-gated Nominal Value) differing only in physical
unit and the exact bit layout of "Location and Status"; Cooling Device
(27) is shaped differently (it references a Temperature Probe Handle
instead of carrying its own Location field) but is still conceptually "a
probe-adjacent sensor/actuator with the same OEM-defined DWORD and
length-gated trailing fields pattern," so it is included in the same task
rather than forced into Task 01 or 02 where it fits worse.

## Key constraints

- Every new class follows the shape already established: constructor,
  type check, `get_type()`/`render_to_description()`, `read_uN` +
  length-guard accessors.
- Voltage/Temperature/Electrical Current Probe's `0x8000` ("unknown")
  sentinel appears on **5 different WORD fields each** (Maximum/Minimum
  Value, Resolution, Tolerance, Accuracy, +Nominal Value for Electrical
  Current specifically) - expose a shared private helper (not a public
  API, just an implementation-detail function each `.cpp` can call) that
  takes a raw `uint16_t` and returns whether it equals the sentinel,
  rather than writing the same `if (value == 0x8000)` five times per
  class with no shared helper at all.
- Cooling Device (27)'s Nominal Speed (0x0C, 2.2+) and Description (0x0E,
  2.7+) fields are each independently length-gated ("present only if
  ...") rather than version-gated alone - guard both on
  `get_entry_size()` thresholds (`> 0x0C`, `>= 0x0F`) exactly as the spec
  states, not solely on `SMBiosVersion`.
- Electrical Current Probe (29)'s Nominal Value (0x14) is likewise
  length-gated ("present ... only if ... length is larger than 14h"), the
  same idiom as Cooling Device's two trailing fields - third confirmed
  instance of "trailing field present only when `Length` says so" in this
  milestone alone.
- System Power Controls (25)'s 5 "Next Scheduled Power-on" fields are
  **BCD-encoded**, not plain binary bytes (7.26.1) - accessors return the
  raw BCD byte; a `bcd_to_decimal()` helper (private, shared across this
  class's 5 field accessors) converts for `render_to_description()` and
  any `get_*_decimal()` convenience accessor this task chooses to add.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/pointing_device_entry.h` /
  `libsmbios/src/pointing_device_entry.cpp` (new)
- `libsmbios/include/smbios/tables/portable_battery_entry.h` /
  `libsmbios/src/portable_battery_entry.cpp` (new)
- `libsmbios/include/smbios/tables/system_reset_entry.h` /
  `libsmbios/src/system_reset_entry.cpp` (new)
- `libsmbios/include/smbios/tables/hardware_security_entry.h` /
  `libsmbios/src/hardware_security_entry.cpp` (new)
- `libsmbios/include/smbios/tables/system_power_controls_entry.h` /
  `libsmbios/src/system_power_controls_entry.cpp` (new)
- `libsmbios/include/smbios/tables/voltage_probe_entry.h` /
  `libsmbios/src/voltage_probe_entry.cpp` (new)
- `libsmbios/include/smbios/tables/cooling_device_entry.h` /
  `libsmbios/src/cooling_device_entry.cpp` (new)
- `libsmbios/include/smbios/tables/temperature_probe_entry.h` /
  `libsmbios/src/temperature_probe_entry.cpp` (new)
- `libsmbios/include/smbios/tables/electrical_current_probe_entry.h` /
  `libsmbios/src/electrical_current_probe_entry.cpp` (new)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all nine.
