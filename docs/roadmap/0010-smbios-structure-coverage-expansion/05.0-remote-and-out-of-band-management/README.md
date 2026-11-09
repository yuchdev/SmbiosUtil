# Story 05.0 - Remote/Out-of-Band Management Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement Out-of-Band Remote Access (30, DSP0134 3.9.0 section 7.31),
Boot Integrity Services Entry Point (31, section 7.32, minimal per its
own content-free spec text), Management Device (34, section 7.35),
Management Device Component (35, section 7.36), Management Device
Threshold Data (36, section 7.37), Memory Channel (37, section 7.38),
IPMI Device Information (38, section 7.39), and Management Controller
Host Interface (42, section 7.43).

## Tasks

| #  | Document                                                         | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Out-of-band remote access and BIS entry point](01-oob-remote-access-and-bis.md) | ✅ Complete | 04     |
| 02 | [Management device, component, and threshold data](02-management-device-family.md) | ✅ Complete | 04     |
| 03 | [Memory channel, IPMI device, and MCTP host interface](03-memory-channel-ipmi-mctp.md) | ✅ Complete | 04     |
| 04 | [Tests](04-tests.md)                                                   | ✅ Done | -      |

Task 01 pairs the two smallest/oldest types in this story: Out-of-Band
Remote Access (30, one string field plus a redacted second row this
task's own spec work must confirm) and the content-free Boot Integrity
Services Entry Point (31). Task 02 is the Management Device "family" -
types 34 (the device itself), 35 (a component of it, referencing 34's
handle), and 36 (threshold data, referenced by 35's own Threshold Handle)
- three structures the spec itself describes as relating to each other
(7.35's own text: "A Management Device might control one or more ... as
defined by one or more Management Device Component structures"), so
implementing them together lets one task's cross-checking work cover all
three handle relationships at once. Task 03 groups Memory Channel (37,
variable-length Load/Handle pair array - same "count byte then N
fixed-width tuples" shape as `GroupAssociationsEntry`'s item array, but
with an explicit count field this time, unlike 14's computed count),
IPMI Device Information (38), and Management Controller Host Interface
(42) - thematically "remote/management interface descriptors," and the
spec's own text (7.39) explicitly cross-references 38 and 42 as
alternative ways to describe the same kind of interface.

## Key constraints

- Every new class follows the shape already established: constructor,
  type check, `get_type()`/`render_to_description()`, `read_uN` +
  length-guard accessors.
- Boot Integrity Services Entry Point (31) is **content-free** per
  DSP0134 3.9.0 - 7.32 reserves the type number and points to an external
  BIS API spec with no field table of its own in this document. Its
  entry class is deliberately minimal: a type check, `get_entry_size()`,
  and a `render_to_description()` that says exactly that (no decoded
  fields) - this is not an oversight or a deferred task, it is the
  complete scope per the spec this project targets (see status.md's own
  note recorded ahead of this story).
- Management Device Component (35)'s Threshold Handle (0x09) and
  Component Handle (0x07) are forward/cross references to structures
  this task does **not** validate exist (no handle-resolution/lookup
  across structures anywhere in this codebase today) - expose the raw
  handle only, matching every other handle-field accessor in this
  codebase's existing precedent (e.g. `PhysicalMemoryArrayEntry`'s
  Memory Error Information Handle).
- Memory Channel (37)'s own spec text is explicit: "this field [Length]
  must not be used to determine the number of memory devices" - the
  Memory Device Count field at 0x06 is the *only* correct source for the
  load/handle array's length, mirroring (in spirit, not mechanism) Group
  Associations' own "don't trust an implicit assumption, use the
  documented derivation" caution, except here the derivation is an
  explicit field, not computed from `Length`.
- Management Controller Host Interface (42)'s own structure is
  explicitly version-sensitive: a pre-3.2 implementation omits the
  Interface Type Specific Data Length field entirely (direct IANA vendor
  ID bytes follow Interface Type when `Interface Type == OEM`), while
  3.2+ adds that length field before the vendor-specific data. **Explicit
  scope cutoff (R8):** this task implements the 3.2+ shape only (Interface
  Type, Interface Type Specific Data Length, the variable-length
  type-specific data it sizes, Number of Protocol Records, and the
  Protocol Records array raw bytes) and documents in the class doc
  comment that pre-3.2 OEM-type structures are decoded using the same
  field layout a 3.2+ reader would use (i.e. not specially detected or
  worked around) - matching this project's existing policy of writing
  against the current spec revision, not every historical revision's
  binary-compatibility quirk.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/out_of_band_remote_access_entry.hpp` /
  `libsmbios/src/out_of_band_remote_access_entry.cpp` (new)
- `libsmbios/include/smbios/tables/boot_integrity_services_entry.hpp` /
  `libsmbios/src/boot_integrity_services_entry.cpp` (new)
- `libsmbios/include/smbios/tables/management_device_entry.hpp` /
  `libsmbios/src/management_device_entry.cpp` (new)
- `libsmbios/include/smbios/tables/management_device_component_entry.hpp` /
  `libsmbios/src/management_device_component_entry.cpp` (new)
- `libsmbios/include/smbios/tables/management_device_threshold_data_entry.hpp` /
  `libsmbios/src/management_device_threshold_data_entry.cpp` (new)
- `libsmbios/include/smbios/tables/memory_channel_entry.hpp` /
  `libsmbios/src/memory_channel_entry.cpp` (new)
- `libsmbios/include/smbios/tables/ipmi_device_information_entry.hpp` /
  `libsmbios/src/ipmi_device_information_entry.cpp` (new)
- `libsmbios/include/smbios/tables/management_controller_host_interface_entry.hpp` /
  `libsmbios/src/management_controller_host_interface_entry.cpp` (new)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all eight.
