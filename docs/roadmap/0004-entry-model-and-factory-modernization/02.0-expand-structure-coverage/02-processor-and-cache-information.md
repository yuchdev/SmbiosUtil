# 02 - Processor and Cache Information

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `ProcessorInformationEntry` (`SMBiosHandler::ProcessorInformation`,
type `4`) and `CacheInformationEntry` (`SMBiosHandler::CacheInformation`,
type `7`) - the two types with the richest revision history after Memory
Device, which makes them the best proof that the field-table pattern (one
`constexpr offset` + one length guard per field, no version subclass) holds
up on a type this involved.

## Files

- **Create** `smbios/include/smbios/processor_information_entry.h` / `.cpp`:
  socket designation, processor type/family/manufacturer, processor ID (8
  bytes), version string, voltage, external/max/current clock speed,
  status, upgrade, and the 2.3+/2.5+/2.6+ additions (L1/L2/L3 cache handle,
  serial number/asset tag/part number string indices, core
  count/enabled/thread count, characteristics bitmask, and the 3.0+
  2-byte-wide core/thread count fields that supersede the original 1-byte
  ones when they read `0xFF`).
- **Create** `smbios/include/smbios/cache_information_entry.h` / `.cpp`:
  socket designation, cache configuration bitmask, max/installed size,
  supported/current SRAM type bitmasks, cache speed, error correction
  type, system cache type, associativity, and the 3.1+ widened
  maximum/installed size fields (the one place this type's own revision
  history mirrors Memory Device's "a later spec widens an existing field"
  pattern - each width tier is one more length-guarded accessor, not a new
  class).
- **Modify** `smbios_util/src/command_line_parser.cpp` / `main.cpp`: add
  display support for both.

## Functions and Classes

- `ProcessorInformationEntry`, `CacheInformationEntry` - new, following
  the same accessor + bitmask-to-string-map pattern as
  `BiosInformationEntry`.

## Constraints

- Processor Information's core/thread-count "0xFF means look at the wider
  2.6+/3.0+ field instead" pattern is exactly the kind of "legacy field is
  a sentinel pointing at a newer, wider field" case
  `BiosInformationEntry::get_rom_size()` already established a precedent
  for (its own `0xFF` → "see extended ROM size" convention) - follow that
  existing precedent rather than inventing a new idiom.

## Tests

- `tests/unit/processor_information_entry_test.cpp`,
  `tests/unit/cache_information_entry_test.cpp` (new): synthetic buffers
  across the documented length tiers, including the 0xFF-sentinel-to-wider-
  field case for both types.

## Success criteria

- [ ] Both types register and resolve through the factory.
- [ ] The 0xFF-sentinel cases are covered by a test proving the wider
      field is consulted correctly.
- [ ] `smbios_util` displays both types on real hardware.
