# Milestone 0010 - SMBIOS Structure Coverage Expansion - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                             | Status         | Tests |
|-------|---------------------------------------------------------------------|----------------|-------|
| 01.0  | `tables/` segment reorg                                             | 🔶 In progress (1/2) | - |
| 02.0  | Board, OEM, and firmware metadata structures                       | ⬜ Not started | -     |
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
