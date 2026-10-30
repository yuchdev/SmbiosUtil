# Story 02.0 - Field-Table Redesign of Versioned Entries

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`byte_reader.h`)

## Scope

Rebuild `BiosInformationEntry`, `MemoryDeviceEntry`, and
`PortConnectionEntry` so every field accessor reads directly from
`raw_data()` (a new protected helper on `AbstractSMBiosEntry`) via
`read_u8`/`read_u16`/`read_u32`/`read_u64` plus a `get_entry_size() >=
offset + width` length guard (contract **C2**), and delete every
`*V2x`/`*V3x` struct and version-gated pointer member these three classes
currently carry.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [BIOS Information field table](01-bios-information-field-table.md)  | ⬜ Not started | 04     |
| 02 | [Memory Device field table](02-memory-device-field-table.md)        | ⬜ Not started | 04     |
| 03 | [Port Connection field table](03-port-connection-field-table.md)     | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                | ⬜ Not started | -      |

## Key constraints

- `AbstractSMBiosEntry` gains one new protected method, `raw_data()`,
  returning the stored `header_->data` pointer - every concrete entry
  class's private `const SomeStruct*` member(s) are deleted in favor of
  calling `raw_data()` directly in each accessor.
- Every deleted struct (`BiosInformationV24`, `BiosInformationV31`,
  `MemoryDeviceV21` through `V28`) and its `#pragma pack(push, 1)` /
  `#pragma pack(pop)` wrapper is removed from the corresponding header -
  not left dead/unused.
- Every public getter's documented `@brief 0xNN offset` comment is
  double-checked against the DMTF SMBIOS spec during conversion (R7) -
  this is the point in the milestone where a long-standing off-by-one in a
  comment, if any exists, would surface.
- `BiosInformationEntry::get_runtime_size()`'s existing arithmetic
  (`(0x10000 - starting_segment) << 4`) and
  `MemoryDeviceEntry::get_form_factor()`/`get_device_type()`'s existing
  `assert(false)` on an unrecognized enum value are **behavior**, not
  **access technique** - preserve them exactly; only how the underlying
  byte is read changes.

## Files or directories created or modified by this story

- `smbios/include/smbios/abstract_smbios_entry.h` /
  `smbios/src/abstract_smbios_entry.cpp` - add `raw_data()`.
- `smbios/include/smbios/bios_information_entry.h` /
  `smbios/src/bios_information_entry.cpp` - struct chain deleted; accessors
  rebuilt.
- `smbios/include/smbios/memory_device_entry.h` /
  `smbios/src/memory_device_entry.cpp` - struct chain deleted; accessors
  rebuilt.
- `smbios/include/smbios/port_connection_entry.h` /
  `smbios/src/port_connection_entry.cpp` - single struct deleted;
  accessors rebuilt (simpler conversion - no version chain to collapse,
  see milestone `status.md`'s resolved note).
