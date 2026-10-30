# 03 - System Slots and Memory Array

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement the remaining four structure types:
`SystemSlotsEntry` (`SMBiosHandler::SystemSlots`, type `9`),
`PhysicalMemoryArrayEntry` (`SMBiosHandler::PhysicalMemoryArray`, type
`16`), `MemoryArrayMappedAddressEntry`
(`SMBiosHandler::MemoryArrayMappedAddress`, type `19`), and
`SystemBootInformationEntry` (`SMBiosHandler::SystemBootInformation`, type
`32`, the smallest of the eight new types - grouped here to balance task
size per the story's Key constraints).

## Files

- **Create** `smbios/include/smbios/system_slots_entry.h` / `.cpp`: slot
  designation string index, slot type, data bus width, current usage,
  length, slot ID, characteristics bitmasks 1/2 (2.1+), segment
  group/bus/device-function number (2.6+).
- **Create** `smbios/include/smbios/physical_memory_array_entry.h` / `.cpp`:
  location, use, memory error correction, maximum capacity (with the
  `0x80000000` sentinel meaning "see extended maximum capacity"), memory
  error information handle, number of memory devices, extended maximum
  capacity (2.7+, 8 bytes).
- **Create** `smbios/include/smbios/memory_array_mapped_address_entry.h` /
  `.cpp`: starting/ending address, memory array handle, partition width,
  and the 2.7+ 8-byte extended starting/ending address pair (same "legacy
  field sentinel → wider field" pattern as `PhysicalMemoryArrayEntry`'s
  maximum capacity).
- **Create** `smbios/include/smbios/system_boot_information_entry.h` /
  `.cpp`: reserved (6 bytes) + boot status data (variable length, 1-9+
  bytes depending on the boot status code's own encoding per spec - this
  is the one new type whose "payload" is not a flat set of fixed-offset
  scalar fields, so its accessor returns the raw status bytes plus a
  decoded status-code enum for the first byte, rather than N separate
  `read_uN` calls).
- **Modify** `smbios_util/src/command_line_parser.cpp` / `main.cpp`: add
  display support for all four.

## Functions and Classes

- `SystemSlotsEntry`, `PhysicalMemoryArrayEntry`,
  `MemoryArrayMappedAddressEntry`, `SystemBootInformationEntry` - new.

## Constraints

- `PhysicalMemoryArrayEntry`'s and `MemoryArrayMappedAddressEntry`'s
  "`0x80000000`/legacy-field sentinel → read the wider field instead"
  pattern follows the same precedent as Task 02's processor core-count
  handling and `BiosInformationEntry::get_rom_size()`'s `0xFF` convention -
  three independent confirmations, by the end of this story, that one
  consistent idiom covers every "legacy narrow field superseded by a wider
  one" case in the whole SMBIOS spec this project touches.
- `SystemBootInformationEntry`'s variable-length boot-status payload must
  still be read through `read_uN` at explicit offsets for its fixed first
  byte (status code) - it is not exempt from R1/R2, only its *trailing*
  vendor/status-specific bytes are opaque/returned raw.

## Tests

- `tests/unit/system_slots_entry_test.cpp`,
  `tests/unit/physical_memory_array_entry_test.cpp`,
  `tests/unit/memory_array_mapped_address_entry_test.cpp`,
  `tests/unit/system_boot_information_entry_test.cpp` (new): synthetic
  buffers per documented length tier, including both sentinel-to-wider-
  field cases.

## Success criteria

- [ ] All four types register and resolve through the factory.
- [ ] Both sentinel-to-wider-field cases are covered by tests.
- [ ] `smbios_util` displays all four types on real hardware.
