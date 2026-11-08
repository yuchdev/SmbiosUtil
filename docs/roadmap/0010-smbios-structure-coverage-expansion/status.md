# Milestone 0010 - SMBIOS Structure Coverage Expansion - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                             | Status         | Tests |
|-------|---------------------------------------------------------------------|----------------|-------|
| 01.0  | `tables/` segment reorg                                             | ✅ Complete (2026-11-08) | 166 unit |
| 02.0  | Board, OEM, and firmware metadata structures                       | ✅ Complete (2026-11-08) | 191 unit |
| 03.0  | Event log and memory error/mapping structures                      | ✅ Complete (2026-11-08) | 237 unit |
| 04.0  | Sensors, probes, and small system-control structures                | 🔶 In progress (2/4 tasks, 2026-11-08) | 274 unit |
| 05.0  | Remote/out-of-band management structures                           | ⬜ Not started | -     |
| 06.0  | Power, onboard-device, and additional-info structures              | ⬜ Not started | -     |
| 07.0  | Security, processor, firmware-inventory, and terminal structures    | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This milestone depends on milestone 0006 (`SyntheticTableBuilder`,
  `tests/unit/synthetic_table_builder.h`) for R6 - already complete as of
  this writing, so no blocking dependency in practice.
- Story 01.0 must land before any of Stories 02.0-07.0 start (see plan.md's
  dependency graph and ADR 0002) - every new entry header is written
  directly into `include/smbios/tables/`, never flat.
- Stories 02.0-07.0 are mutually independent and may run in any order or
  in parallel; plan.md flags `smbios/src/command_line_parser.cpp`/
  `main.cpp` (R7) as a shared-file merge-conflict risk if run concurrently,
  not a logical dependency.
- Milestone 0009 (structured output formats) is independent and, as of
  this writing, also "Not started." If it lands before this milestone's
  later stories, no coordination is assumed here - see plan.md's Non-goals.
- Type 31 (Boot Integrity Services Entry Point) has no field table of its
  own in DSP0134 3.9.0 (7.32 only reserves the type number and points to an
  external BIS API spec) - its entry class (Story 05.0, Task 01) is
  necessarily minimal: a type check and `get_entry_size()`, no decoded
  fields beyond what Annex 4.10's checksum/entry-point validation notes
  describe. Documented here ahead of implementation so this is not
  mistaken for an oversight when Story 05.0 lands.

## Story 02.0 - Board, OEM, and Firmware Metadata Structures (✅ 2026-11-08)

**Delivered:** 5 new entry types registered and verified through the factory - `BaseboardInformationEntry` (2), `OemStringsEntry` (11), `SystemConfigurationOptionsEntry` (12), `FirmwareLanguageInformationEntry` (13), `GroupAssociationsEntry` (14). Each added its own symbolic `SMBios::SMBiosHandler` enumerator (no raw literals), full Doxygen coverage, and per-type synthetic-buffer tests via `SyntheticTableBuilder`. Task 04 added all 5 to the existing consolidated `CreateResolvesEveryRealSMBiosHandlerExceptEndOfTableTestCase` in `tests/unit/smbios_entry_factory_test.cpp` - confirmed resolving with a header-only (Length=4) minimal buffer, same as the original 11 types.

**Spec discrepancies found and corrected against the real spec (not task-doc reproductions):** Task 01 found the "minimum length" test case's string-field count was off by one byte vs. the real 7.3 byte layout. Task 02 found Firmware Language Information's trailing language-string list does NOT start after `Current Language`'s own index as the task doc implied - `Current Language` is itself a 1-based index INTO that same string list, confirmed against the spec's own worked examples. Task 03's spec reproduction was accurate (no discrepancy).

**Tests / gate:** `smbios_unit_test` 191 cases, 0 failures (up from 166 before this story). All 4 `ctest` targets green throughout, independently re-verified after every task.

**Known issue carried forward, not fixed in this story:** the `smbios` CLI never actually links in any entry class's `REGISTER_SMBIOS_ENTRY` static registration (confirmed via `nm`) and so prints raw header IDs instead of decoded content on real hardware - true for all 11 pre-existing types too, not introduced by this story. Flagged as a separate task (`task_dc27f15d`) rather than fixed inline, since it's a CLI-target link-configuration issue orthogonal to adding new entry types.

## Story 03.0 - Event Log and Memory Error/Mapping Structures (✅ 2026-11-08)

**Task 01 delivered:** `SystemEventLogEntry` (`SMBios::SystemEventLog` = 15, DSP0134 3.9.0 7.16) - fixed header fields (Log Area Length, Log Header/Data Start Offsets, Access Method, Log Status bitmask, Log Change Token, Access Method Address) plus the 2.1+ Log Header Format/descriptor-count(`x`)/descriptor-length(`y`)/trailing-descriptor-list fields, each guarded by both `SMBiosVersion >= {2,1}` and `get_entry_size()`. Explicit scope cutoff (R8) documented in the class doc comment: the actual non-volatile event-log records the structure points to via Access Method Address are out of scope - this codebase has no OS abstraction for indexed-I/O/memory-mapped/GPNV access.

**Spec discrepancy check:** the task document's own reproduction of Table 60 redacted the 0x0A "Access Method" row (a markdown-extraction artifact). Read directly from `docs/standard/DSP0134_3.9.0.md`'s own 7.16, it confirmed Access Method is exactly where the surrounding fields' offset arithmetic implied - a BYTE at 0x0A, between Log Data Start Offset (ending at 0x0A) and Log Status (0x0B) - with 5 defined enum values (00h-04h), 05h-7Fh reserved, 80h-FFh OEM-specific. No real discrepancy; the redaction was resolved by reading the full table, not by guessing.

**Tests / gate:** `tests/unit/system_event_log_entry_test.cpp` (new, 5 cases) - a version-2.0-shaped buffer (Length==0x14, confirming 2.1+ accessors report unavailable), a 2.1+ buffer with `x==0` descriptors (confirmed as the normal "list not present" case per the spec's own note, not an error), a 2.1+ buffer with 2 descriptors (confirming the trailing-list offset arithmetic uses the actually-read `y` value, not a hardcoded 2), a pre-2.1-version guard case with a long-enough buffer, and a factory-resolution case. `smbios_unit_test` 196 cases, 0 failures (up from 191 before this task). All 4 `ctest` targets green.

**CLI:** `smbios/src/main.cpp` includes `system_event_log_entry.h` for display, mirroring existing precedent; the CLI-linking bug (`task_dc27f15d`) is unchanged by this task.

**Task 02 delivered:** `MemoryErrorInformation32BitEntry` (`SMBios::MemoryErrorInformation32Bit` = 18, DSP0134 3.9.0 7.19, Table 81) and `MemoryErrorInformation64BitEntry` (`SMBios::MemoryErrorInformation64Bit` = 33, 7.34, Table 106) - deliberately not sharing a base beyond `AbstractSMBiosEntry`, each with its own verbatim-duplicated copy of the Error Type/Granularity/Operation lookup tables (7.19.1/7.19.2/7.19.3), referenced via `@see` rather than re-derived. Both expose `get_error_type()`/`get_error_granularity()`/`get_error_operation()` (+ `_string()` variants), `get_vendor_syndrome()` + `is_vendor_syndrome_known()` (all-zero sentinel), and known/unknown predicates for every address/resolution field at its own width.

**Spec discrepancy check:** the task document's own claim that the 64-bit structure's Error Resolution field (offset 0x1B) stays DWORD-wide (`8000 0000h` sentinel) rather than widening to QWORD like the two address fields was verified directly against Table 106 in `docs/standard/DSP0134_3.9.0.md` - confirmed accurate, not a transcription error. `MemoryErrorInformation64BitEntry::get_error_resolution()` returns `uint32_t`, matching the real spec table, with its own dedicated test (`ErrorResolutionUnknownSentinelTestCase`) asserting the DWORD sentinel specifically (not the QWORD one used by the two address fields) is what triggers "unknown."

**Tests / gate:** `tests/unit/memory_error_information_32bit_entry_test.cpp` and `tests/unit/memory_error_information_64bit_entry_test.cpp` (new, 14 cases each) - full-field decode, every documented Error Type/Granularity/Operation enum value, a distinct known-value and sentinel case for each of Vendor Syndrome/Memory Array Error Address/Device Error Address/Error Resolution, a truncated-structure bounds-guard case, and a factory-resolution case. `smbios_unit_test` 224 cases, 0 failures (up from 196 before this task). All 4 `ctest` targets green.

**CLI:** `smbios/src/main.cpp` includes both new headers for display, mirroring existing precedent; the CLI-linking bug (`task_dc27f15d`) is unchanged by this task.

**Task 03 delivered:** `MemoryDeviceMappedAddressEntry` (`SMBios::MemoryDeviceMappedAddress` = 20, DSP0134 3.9.0 7.21, Table 86) - Starting/Ending Address (legacy DWORD, kibibytes, `0xFFFFFFFF` sentinel -> 2.7+ Extended Starting/Ending Address QWORD, bytes), Memory Device Handle, Memory Array Mapped Address Handle, Partition Row Position (0 = reserved, 0xFF = unknown - two distinct states), Interleave Position (0 = non-interleaved - a valid value with a different meaning than Partition Row Position's reserved 0 - 0xFF = unknown). Follows `PhysicalMemoryArrayEntry`'s/`MemoryArrayMappedAddressEntry`'s established sentinel-widening pattern, but additionally normalizes the unit itself: `get_starting_address()`/`get_ending_address()` always return bytes, converting the legacy kibibyte value (`* 1024`) rather than leaving the caller to reconcile two different units across the sentinel branch.

**Spec discrepancy check:** verified Table 86 directly against `docs/standard/DSP0134_3.9.0.md` - field offsets/sentinels/units in the task document matched exactly, including the easily-missed Interleaved Data Depth field at 0x12 (BYTE, 2.1+, between Interleave Position and the 2.7+ extended fields) which the task's own field list omits but which does not shift the extended-field offsets (0x13/0x1B), since the task document's own offsets already accounted for it. No real discrepancy found this task.

**Tests / gate:** `tests/unit/memory_device_mapped_address_entry_test.cpp` (new, 13 cases) - truncated-structure defaults, a 2.1-shaped buffer with an explicit kibibyte-to-byte conversion value-equality assertion (`0x10` KiB -> `0x4000` bytes, `0x20` KiB -> `0x8000` bytes), a 2.7+-shaped buffer with a real non-sentinel legacy address (confirms the legacy value, converted, is used even though non-zero extended fields are present in the buffer), a 2.7+-shaped buffer with the sentinel set (confirms the extended QWORD fields are read instead), a length-insufficient sentinel edge case, three Partition Row Position cases (0/Reserved, 0xFF/Unknown, normal), three Interleave Position cases (0/Non-interleaved, 0xFF/Unknown, normal), a wrong-header-type throw case, and a factory-resolution case. `smbios_unit_test` 237 cases, 0 failures (up from 224 before this task). All 4 `ctest` targets green.

**CLI:** `smbios/src/main.cpp` includes the new header for display, mirroring existing precedent; the CLI-linking bug (`task_dc27f15d`) is unchanged by this task.

Task 04 added types 15, 18, 20, 33 to the consolidated factory-resolution test in `tests/unit/smbios_entry_factory_test.cpp` - all resolve correctly from a minimal header-only buffer. Full suite: 237 unit cases, 4/4 ctest targets green, independently re-verified after every task. Story 03.0 complete.

## Story 04.0 - Sensors, Probes, and Small System-Control Structures (🔶 in progress, 2026-11-08)

**Task 01 delivered:** `PointingDeviceEntry` (`SMBios::PointingDevice` = 21, DSP0134 3.9.0 7.22, Table 87) - Device Type (7.22.1, Table 88) and Interface (7.22.2, Table 89) enums plus Number of Buttons, with the accessor deliberately named `get_device_type()` (never `get_type()`) to avoid shadowing the inherited structure-kind accessor. `PortableBatteryEntry` (`SMBios::PortableBattery` = 22, 7.23, Table 90) - Location/Manufacturer/Device Name/SBDS Version strings, Manufacture Date and Serial Number strings each with a `uses_sbds_manufacture_date()`/`uses_sbds_serial_number()` predicate for the "index 0 means see SBDS instead" 2.2+ convention (SBDS data itself not decoded), Device Chemistry enum (7.23.1, Table 91), Design Capacity (mWh) and Design Voltage (mV) each with a known/unknown predicate, and Maximum Error percentage.

**Design Capacity Multiplier resolution (the task's own flagged open question):** read directly from `docs/standard/DSP0134_3.9.0.md`'s full Table 90, the Design Capacity Multiplier IS a separate structure field - offset 0x15h, BYTE, 2.2+, immediately after SBDS Device Chemistry (0x14h) and before the OEM-specific DWORD (0x16h) - not merely prose about external interpretation. `PortableBatteryEntry::get_design_capacity()` therefore returns the EFFECTIVE value: the raw 0x0A WORD multiplied by the 0x15 BYTE when that field is present (`get_entry_size() >= 0x16`), defaulting the multiplier to 1 (the spec's own documented default) both when the field is absent on a shorter/pre-2.2 structure and defensively if present-but-zero. The 0x0A field's own "0 means unknown" sentinel is checked pre-multiplication. The raw multiplier byte itself is not exposed as a separate public accessor, since the task's accessor list calls only for the already-resolved effective capacity; it is used internally via a private helper.

**Tests / gate:** `tests/unit/pointing_device_entry_test.cpp` (new, 6 cases) and `tests/unit/portable_battery_entry_test.cpp` (new, 12 cases) - every documented Device Type/Interface/Device Chemistry enum value, the SBDS-indicator (index 0) cases for Manufacture Date and Serial Number as cases distinct from the normal-string case, the unknown sentinels for Design Capacity (0)/Design Voltage (0)/Maximum Error (0xFF) each as a distinct case, the Design Capacity Multiplier applying when present vs. defaulting to 1 when absent (also distinct cases), a minimum-length-buffer defaults case, an unrecognized-enum-value case, and a factory-resolution case, for each type. `smbios_unit_test` 255 cases, 0 failures (up from 237 before this task). All 4 `ctest` targets green.

**CLI:** `smbios/src/main.cpp` includes both new headers for display, mirroring existing precedent; the CLI-linking bug (`task_dc27f15d`) is unchanged by this task.

Types 21/22 are not yet added to the consolidated factory-resolution test in `tests/unit/smbios_entry_factory_test.cpp` - deferred to this story's own Task 04 (Tests), matching Story 03.0's precedent of doing that consolidation in its own final task rather than per-task.

**Task 02 delivered:** `SystemResetEntry` (`SMBios::SystemReset` = 23, 7.24, Table 92), `HardwareSecurityEntry` (`SMBios::HardwareSecurity` = 24, 7.25, Table 93), `SystemPowerControlsEntry` (`SMBios::SystemPowerControls` = 25, 7.26, Table 94).

**Bit-layout confirmation (the task's own flagged open question):** this task's own source excerpt redacted both Table 92's Capabilities row and Table 93's Hardware Security Settings row, so both were re-read in full directly from `docs/standard/DSP0134_3.9.0.md` before any decoding logic was written. Confirmed layouts:
- Table 92 (System Reset Capabilities, 0x04): Bits 7:6 Reserved; Bit 5 Watchdog timer present; Bits 4:3 Boot Option on Limit; Bits 2:1 Boot Option; Bit 0 Status (user-enabled). Boot Option and Boot Option on Limit share the identical 2-bit value space (00b Reserved/do not use, 01b Operating system, 10b System utilities, 11b Do not reboot), so both are exposed through one `SystemResetEntry::BootOption` enum.
- Table 93 (Hardware Security Settings, 0x04): Bits 7:6 Power-on Password Status; Bits 5:4 Keyboard Password Status; Bits 3:2 Administrator Password Status; Bits 1:0 Front Panel Reset Status - all four sharing the identical 4-value `HardwareSecurityEntry::SecurityStatus` enum (00b Disabled, 01b Enabled, 10b Not Implemented, 11b Unknown), exactly the "informed guess" the task doc itself flagged as unconfirmed, now verified against the real table.

`SystemResetEntry` also exposes Reset Count/Reset Limit/Timer Interval (minutes)/Timeout (minutes), each a WORD with `0xFFFF` ("unknown") as a distinct, independently-tested sentinel. `SystemPowerControlsEntry` (7.26/7.26.1, Table 94 - not redacted in the task excerpt, so no bit-layout confirmation was needed, only whole-byte BCD fields) exposes raw BCD accessors plus decimal convenience accessors via a shared private `bcd_to_decimal()` helper performing the mechanical `(b >> 4) * 10 + (b & 0x0F)` nibble-pair conversion with no range validation, documented as such.

**Tests / gate:** `tests/unit/system_reset_entry_test.cpp` (10 cases: every Boot Option/Boot Option on Limit value decoded independently, a Status-disabled/Watchdog-absent case, the 4 WORD unknown-sentinel cases each as its own test, a known-values round-trip, a minimum-length-buffer defaults case, a wrong-type-throws case, a factory-resolution case), `tests/unit/hardware_security_entry_test.cpp` (4 cases: an exhaustive 4^4 = 256-combination sweep over all four SecurityStatus sub-fields confirming no bit-position aliasing, a minimum-length-buffer defaults case, a wrong-type-throws case, a factory-resolution case), `tests/unit/system_power_controls_entry_test.cpp` (5 cases: a real BCD date/time decode with explicit per-field value-equality assertions including `0x25` BCD → decimal `25`, a minimum-length-buffer defaults case, a malformed-0xFF-byte mechanical-decode case, a wrong-type-throws case, a factory-resolution case). `smbios_unit_test` 274 cases, 0 failures (up from 255 before this task). All 4 `ctest` targets green.

**CLI:** `smbios/src/main.cpp` includes all three new headers for display, mirroring existing precedent; the CLI-linking bug (`task_dc27f15d`) is unchanged by this task.
