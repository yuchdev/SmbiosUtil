# Story 01.0 - `tables/` Segment Reorg

**Parent milestone:** [plan.md](../plan.md)
**Status:** 🔶 In progress (1/2 tasks)
**Depends on:** ADR [0002](/docs/adr/0002-tables-segment-for-smbios-entry-types.md)

## Scope

Relocate the 11 existing SMBIOS structure-type entry headers from
`libsmbios/include/smbios/*.h` to `libsmbios/include/smbios/tables/*.h`,
per ADR 0002's decision. Corresponding `.cpp` files stay flat in
`libsmbios/src/` (unchanged location, only their own `#include` line
changes). Zero logic/behavior change - this is a mechanical move plus
include-path update, verified by the full existing test suite passing
unmodified.

## Tasks

| #  | Document                                                   | Status         | Blocks |
|----|-------------------------------------------------------------|----------------|--------|
| 01 | [Relocate headers and update build](01-relocate-headers-and-update-build.md) | ✅ Done        | 02     |
| 02 | [Tests](02-tests.md)                                          | ⬜ Not started | -      |

Two tasks, not more: the move itself (Task 01) and verifying nothing broke
(Task 02) - there is no third concern this story needs to split out,
unlike the feature stories that follow it.

## Key constraints

- Every one of the 11 moved headers keeps its exact filename, only its
  directory changes (`bios_information_entry.h` ->
  `tables/bios_information_entry.h`, etc.) - no renaming, no content
  changes beyond its own `#include` lines if it includes a sibling entry
  header (none currently do, per a check of the existing 11 headers'
  `#include` lists).
- `libsmbios/include/smbios/*.h`'s 4 non-entry files
  (`smbios.h`, `abstract_smbios_entry.h`, `smbios_entry_factory.h`,
  `smbios_entry_interface.h`) stay exactly where they are - ADR 0002's
  Decision point 3 is explicit that `tables/` is a segment for
  structure-type entries only.
- `smbios_entry_factory.{h,cpp}` need no edit at all (ADR 0002's point 5) -
  if this task finds itself editing either file, that is a signal the task
  has drifted from the ADR's design, not a normal part of the move.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/` (new directory) - receives all 11
  moved headers: `bios_information_entry.h`, `system_information_entry.h`,
  `system_enclosure_entry.h`, `processor_information_entry.h`,
  `cache_information_entry.h`, `port_connection_entry.h`,
  `system_slots_entry.h`, `physical_memory_array_entry.h`,
  `memory_device_entry.h`, `memory_array_mapped_address_entry.h`,
  `system_boot_information_entry.h`.
- `libsmbios/src/*.cpp` (11 files, unchanged location) - each one's own
  `#include <smbios/xxx_entry.h>` updated to
  `#include <smbios/tables/xxx_entry.h>`.
- `libsmbios/src/CMakeLists.txt` - header glob gains a second line for
  `tables/*.h` (ADR 0002's Decision point 4).
- `smbios/src/command_line_parser.cpp` / `main.cpp` - any `#include` of an
  entry header updated to the new path.
- `tests/unit/*_entry_test.cpp` (every file that includes an entry header
  directly) - `#include` path updated; no assertion changes.
