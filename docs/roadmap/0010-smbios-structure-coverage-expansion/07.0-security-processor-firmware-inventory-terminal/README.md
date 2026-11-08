# Story 07.0 - Security, Processor, Firmware-Inventory, and Terminal Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** 🔶 In progress (3/4 tasks)
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement TPM Device (43, DSP0134 3.9.0 section 7.44), Processor
Additional Information (44, section 7.45), Firmware Inventory
Information (45, section 7.46), String Property (46, section 7.47), and
Inactive (126, section 7.48, tacked on here per plan.md's Non-goals - no
dedicated task of its own). This is the milestone's last story - every
one of the 34 non-obsolete remaining types is covered once this story
lands.

## Tasks

| #  | Document                                                  | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [TPM device](01-tpm-device.md)                                      | ✅ Complete | 04     |
| 02 | [Processor additional information](02-processor-additional-information.md) | ✅ Complete | 04     |
| 03 | [Firmware inventory, string property, and inactive](03-firmware-inventory-string-property-inactive.md) | ✅ Complete | 04     |
| 04 | [Tests](04-tests.md)                                              | ✅ Done     | -      |

Task 01 is TPM Device (43) alone - a flat, fully fixed-offset structure
with its own QWORD characteristics bitmask (7.44.1), self-contained.
Task 02 is Processor Additional Information (44) alone - its
variable-length, vendor-defined "processor-specific block" (7.45.1-2) is
the second-most-open-ended payload in this milestone after System Event
Log (Story 03.0), warranting its own explicit scope-cutoff treatment
rather than being batched where that nuance could get lost. Task 03
closes the milestone with Firmware Inventory Information (45, the
spec's newest structure among this milestone's set, added in 3.5),
String Property (46, also 3.5, a small cross-reference "add a property
to another structure" type), and Inactive (126, a one-line addition with
no fields of its own, per plan.md's explicit Non-goals decision not to
give it a dedicated task).

## Key constraints

- Every new class follows the shape already established: constructor,
  type check, `get_type()`/`render_to_description()`, `read_uN` +
  length-guard accessors.
- TPM Device (43)'s Vendor ID (0x04, 4 raw ASCII bytes, **not** a DMI
  string index) is read as 4 raw bytes and rendered as a string by the
  entry class itself (trimming a trailing `0x00` for 3-character vendor
  IDs per the spec's own "ABC" example) - this is the one STRING-shaped
  field in this entire milestone that is not a DMI string-table index;
  flag this distinction explicitly in the accessor's Doxygen comment so
  it is not mistaken for a `dmi_string()` call site by a future reader
  skimming the class.
- Processor Additional Information (44)'s Processor-Specific Block
  (7.45.1) has its own internal `{Block Length, Processor Type,
  Processor-Specific Data}` sub-header - this class decodes that
  sub-header (Block Length, Processor Type enum) but treats
  Processor-Specific Data itself as opaque raw bytes (7.45.2's own
  per-architecture formats are "maintained by processor architecture
  workgroups or vendors in separate documents," explicitly outside
  DSP0134 3.9.0's own scope, let alone this project's). **Explicit scope
  cutoff (R8)**, same reasoning as System Event Log's (Story 03.0) and
  Additional Information's (Story 06.0) own documented cutoffs.
- Firmware Inventory Information (45)'s Number of Associated Components
  field (0x17) is, per the spec's own explicit warning (same wording
  pattern as Memory Channel's, Story 05.0), the *only* correct source for
  the trailing handle array's length - never derive it from `Length`.
- Inactive (126) gets the same minimal treatment as Boot Integrity
  Services Entry Point (31, Story 05.0): no decoded fields beyond the
  inherited header, `get_type()` returns "Inactive," and
  `render_to_description()` states the structure should be bypassed by
  upper-level software per the spec's own guidance (7.48) - this is a
  one-line addition inside Task 03's own `.cpp`/`.h` pair, not a separate
  file pair of its own size-wise, though it is still its own named class
  (`InactiveEntry`) for factory-registration purposes.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/tpm_device_entry.h` /
  `libsmbios/src/tpm_device_entry.cpp` (new)
- `libsmbios/include/smbios/tables/processor_additional_information_entry.h` /
  `libsmbios/src/processor_additional_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/firmware_inventory_information_entry.h` /
  `libsmbios/src/firmware_inventory_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/string_property_entry.h` /
  `libsmbios/src/string_property_entry.cpp` (new)
- `libsmbios/include/smbios/tables/inactive_entry.h` /
  `libsmbios/src/inactive_entry.cpp` (new, minimal)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all five.
