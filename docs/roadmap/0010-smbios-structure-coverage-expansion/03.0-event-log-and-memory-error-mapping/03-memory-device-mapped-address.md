# 03 - Memory Device Mapped Address

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `MemoryDeviceMappedAddressEntry` (`SMBiosHandler` value `20`),
DSP0134 3.9.0 section 7.21.

## Files

- **Create**
  `libsmbios/include/smbios/tables/memory_device_mapped_address_entry.h` /
  `.cpp`:
  - 0x04 Starting Address (DWORD, kibibytes; `0xFFFFFFFF` sentinel ->
    Extended Starting Address, 2.7+)
  - 0x08 Ending Address (DWORD; `0xFFFFFFFF` sentinel -> Extended Ending
    Address, 2.7+)
  - 0x0C Memory Device Handle (WORD)
  - 0x0E Memory Array Mapped Address Handle (WORD)
  - 0x10 Partition Row Position (BYTE; `0xFF` = unknown)
  - 0x11 Interleave Position (BYTE; `0` = non-interleaved, `0xFF` =
    unknown)
  - 0x13 Extended Starting Address (QWORD, bytes, 2.7+)
  - 0x1B Extended Ending Address (QWORD, bytes, 2.7+)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `MemoryDeviceMappedAddressEntry` - new. `get_starting_address()` (reads
  the extended field transparently when the legacy field is the
  `0xFFFFFFFF` sentinel - same "one accessor, resolves the sentinel
  itself" pattern `PhysicalMemoryArrayEntry::get_maximum_capacity()`
  already established, per this story's own Key constraints note),
  `get_ending_address()` (same pattern), `get_memory_device_handle()`,
  `get_memory_array_mapped_address_handle()`,
  `get_partition_row_position()`, `get_interleave_position()`.

## Constraints

- Follow `PhysicalMemoryArrayEntry`'s and `MemoryArrayMappedAddressEntry`'s
  existing precedent exactly for the sentinel-widening pattern: one
  accessor per logical field, internally choosing the narrow or wide
  storage based on the sentinel, not two separate accessors the caller
  must manually reconcile.
- Starting/Ending Address units differ by width per the spec's own text:
  the legacy (DWORD) fields are in **kibibytes**, the extended (QWORD,
  2.7+) fields are in **bytes** - `get_starting_address()`/
  `get_ending_address()` must normalize to one unit (bytes recommended,
  since that is the wider field's unit and avoids precision loss) rather
  than silently returning raw kibibytes in the legacy case and raw bytes
  in the extended case.
- Partition Row Position's value `0` is explicitly reserved per spec (not
  a valid position) - `get_partition_row_position()` returns the raw
  byte; rendering code (CLI, `render_to_description()`) should surface
  "Reserved" for `0` and "Unknown" for `0xFF` distinctly, matching how
  other reserved/unknown sentinel pairs in this codebase are already kept
  visually distinct (e.g. `SystemSlotsEntry`'s "Unknown" vs "None").

## Tests

- `tests/unit/memory_device_mapped_address_entry_test.cpp` (new): a
  2.1-shaped buffer (`Length == 0x13`, no extended fields), a 2.7+-shaped
  buffer with the legacy sentinel set (reads through to the extended
  fields), and a 2.7+-shaped buffer with a real (non-sentinel) legacy
  address (extended fields present but zero per spec, not read).

## Success criteria

- [ ] Registers and resolves through the factory for `SMBiosHandler == 20`.
- [ ] Sentinel-to-extended-field resolution covered by a test case, unit
      normalization verified (kibibytes vs bytes) by an explicit
      value-equality assertion, not just "does not crash."
- [ ] `smbios` CLI displays the type on real hardware where present.
