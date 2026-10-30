# Story 02.0 - Expand Structure Coverage

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (self-registering factory); Milestone 0003 (accessor pattern)

## Scope

Implement the eight `SMBios::SMBiosHandler` structure types the prototype
does not cover yet, each as a new `AbstractSMBiosEntry` subclass written
directly against milestone 0003's `read_uN` + length-guard pattern and
self-registered per Story 01.0 - no `reinterpret_cast`, no version-named
struct, from the first line of each new class.

## Tasks

| #  | Document                                                              | Status         | Blocks |
|----|----------------------------------------------------------------------------|----------------|--------|
| 01 | [System Information and Enclosure](01-system-information-and-enclosure.md)  | ⬜ Not started | 04     |
| 02 | [Processor and Cache Information](02-processor-and-cache-information.md)    | ⬜ Not started | 04     |
| 03 | [System Slots and Memory Array](03-system-slots-and-memory-array.md)        | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                        | ⬜ Not started | -      |

Grouped by implementation session rather than one task per type (eleven
total entry types fit this project's existing granularity of "a handful of
closely-related classes per task," matching how `smbios_entry_factory.h`
already imports `bios_information_entry.h`, `port_connection_entry.h`, and
`memory_device_entry.h` side by side today). `SystemBootInformation`
(enum value `32`) is included in Task 03 as the smallest of the remaining
types, to balance task size.

## Key constraints

- Every new class follows the exact shape `BiosInformationEntry`/
  `MemoryDeviceEntry`/`PortConnectionEntry` have after milestone 0003: a
  constructor taking `(const DMIHeader&, const SMBiosVersion&)`, a type
  check against its own `SMBiosHandler` value, `get_type()`/
  `render_to_description()` overrides, and field getters reading via
  `read_uN(raw_data(), offset)` behind a `get_entry_size() >= offset +
  width` guard.
- Field layouts and offsets are taken from the DMTF SMBIOS specification
  directly for each type - not reverse-engineered from this project's
  `c-versions/` dmidecode reference alone, though that reference remains
  useful for cross-checking enum value tables (connector types, boot
  status codes, etc.) the way `PortConnectionEntry`'s existing enums
  already do.
- `smbios_util`'s CLI (`command_line_parser.cpp`/`main.cpp`) gains display
  support for each new type as part of its own task, not as an afterthought
  bolted onto the last task.

## Files or directories created or modified by this story

- `smbios/include/smbios/system_information_entry.h` /
  `smbios/src/system_information_entry.cpp` (new)
- `smbios/include/smbios/system_enclosure_entry.h` /
  `smbios/src/system_enclosure_entry.cpp` (new)
- `smbios/include/smbios/processor_information_entry.h` /
  `smbios/src/processor_information_entry.cpp` (new)
- `smbios/include/smbios/cache_information_entry.h` /
  `smbios/src/cache_information_entry.cpp` (new)
- `smbios/include/smbios/system_slots_entry.h` /
  `smbios/src/system_slots_entry.cpp` (new)
- `smbios/include/smbios/physical_memory_array_entry.h` /
  `smbios/src/physical_memory_array_entry.cpp` (new)
- `smbios/include/smbios/memory_array_mapped_address_entry.h` /
  `smbios/src/memory_array_mapped_address_entry.cpp` (new)
- `smbios/include/smbios/system_boot_information_entry.h` /
  `smbios/src/system_boot_information_entry.cpp` (new)
- `smbios_util/src/command_line_parser.cpp` / `main.cpp` - display support
  for all eight.
