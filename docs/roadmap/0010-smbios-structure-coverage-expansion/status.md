# Milestone 0010 - SMBIOS Structure Coverage Expansion - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                             | Status         | Tests |
|-------|---------------------------------------------------------------------|----------------|-------|
| 01.0  | `tables/` segment reorg                                             | ✅ Complete (2026-11-08) | 166 unit |
| 02.0  | Board, OEM, and firmware metadata structures                       | ✅ Complete (2026-11-08) | 191 unit |
| 03.0  | Event log and memory error/mapping structures                      | ⬜ Not started | -     |
| 04.0  | Sensors, probes, and small system-control structures                | ⬜ Not started | -     |
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
