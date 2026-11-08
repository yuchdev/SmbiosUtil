# 02 - 32-Bit and 64-Bit Memory Error Information

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `MemoryErrorInformation32BitEntry` (`SMBiosHandler` value `18`),
DSP0134 3.9.0 section 7.19, and `MemoryErrorInformation64BitEntry` (value
`33`), section 7.34.

## Files

- **Create**
  `libsmbios/include/smbios/tables/memory_error_information_32bit_entry.h` /
  `.cpp`:
  - 0x04 Error Type (BYTE, ENUM - see 7.19.1)
  - 0x05 Error Granularity (BYTE, ENUM - see 7.19.2)
  - 0x06 Error Operation (BYTE, ENUM - see 7.19.3)
  - 0x07 Vendor Syndrome (DWORD, unknown sentinel `0x00000000`)
  - 0x0B Memory Array Error Address (DWORD, unknown sentinel
    `0x80000000`)
  - 0x0F Device Error Address (DWORD, unknown sentinel `0x80000000`)
  - 0x13 Error Resolution (DWORD, unknown sentinel `0x80000000`)
- **Create**
  `libsmbios/include/smbios/tables/memory_error_information_64bit_entry.h` /
  `.cpp`:
  - 0x04 Error Type (BYTE, same enum as above, 7.19.1)
  - 0x05 Error Granularity (BYTE, same enum, 7.19.2)
  - 0x06 Error Operation (BYTE, same enum, 7.19.3)
  - 0x07 Vendor Syndrome (DWORD, unknown sentinel `0x00000000`)
  - 0x0B Memory Array Error Address (QWORD, unknown sentinel
    `0x8000000000000000`)
  - 0x13 Device Error Address (QWORD, unknown sentinel
    `0x8000000000000000`)
  - 0x1B Error Resolution (DWORD, unknown sentinel `0x80000000`)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for both.

## Functions and Classes

- `MemoryErrorInformation32BitEntry` - new. `get_error_type()` +
  `get_error_type_string()`, `get_error_granularity()` +
  `get_error_granularity_string()`, `get_error_operation()` +
  `get_error_operation_string()`, `get_vendor_syndrome()` +
  `is_vendor_syndrome_known()`, `get_memory_array_error_address()` +
  `is_memory_array_error_address_known()`, `get_device_error_address()` +
  `is_device_error_address_known()`, `get_error_resolution()` +
  `is_error_resolution_known()`.
- `MemoryErrorInformation64BitEntry` - new, same accessor shape at QWORD
  width for the two address fields (`get_memory_array_error_address()`/
  `get_device_error_address()` return `uint64_t`; `get_error_resolution()`
  stays DWORD/`uint32_t` per the spec table above - confirm this
  asymmetry is intentional, not a transcription error, against DSP0134
  3.9.0's own Table 106 directly, since it looks unusual for a "64-bit"
  structure to still carry a 32-bit-wide resolution field).
- Shared enum tables (Error Type 7.19.1, Error Granularity 7.19.2, Error
  Operation 7.19.3) are duplicated verbatim between the two classes
  (matching this codebase's existing precedent of "small, type-specific
  lookup maps per class," not a shared enum header - see `README.md`'s
  own note on `OemStringsEntry`/`SystemConfigurationOptionsEntry` not
  sharing a base purely for incidental shape similarity; the same
  reasoning applies here, with the added point that the spec itself
  defines these tables once and references them by section number from
  both structures, which this task's Doxygen comments should mirror via a
  `@see 7.19.1` reference rather than re-deriving the values from first
  principles each time).

## Constraints

- Every "unknown" sentinel predicate (`is_*_known()`) must check the exact
  sentinel value per field width - `0x80000000` for 32-bit addresses,
  `0x8000000000000000` for 64-bit addresses, `0x00000000` for vendor
  syndrome (a different sentinel convention: zero, not the high bit,
  since the all-zero *value* for vendor syndrome is itself meaningless
  data, not a real zero result).
- Error Resolution's sentinel is `0x80000000` on **both** classes (DWORD
  width is unchanged between the 32-bit and 64-bit structures, per Table
  106) - do not widen this field's sentinel check to QWORD on the 64-bit
  class by analogy with the other two address fields; follow the table as
  written.

## Tests

- `tests/unit/memory_error_information_32bit_entry_test.cpp`,
  `tests/unit/memory_error_information_64bit_entry_test.cpp` (new):
  synthetic buffers covering each enum's documented values, each
  sentinel-vs-known-value case for all four width-appropriate address/
  syndrome/resolution fields.

## Success criteria

- [x] Both types register and resolve through the factory
      (`SMBiosHandler` values 18 and 33).
- [x] Every sentinel/known-value case is covered by a distinct test case
      per field.
- [x] `smbios` CLI displays both types on real hardware where present (via
      `SMBiosEntryFactory`'s generic dispatch; `main.cpp` includes both new
      headers so their `REGISTER_SMBIOS_ENTRY` static-init registration is
      linked in - subject to the pre-existing CLI-linking bug tracked
      separately as `task_dc27f15d`, not fixed by this task).
