# 01 - TPM Device

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `TpmDeviceEntry` (`SMBiosHandler` value `43`), DSP0134 3.9.0
section 7.44, through its Characteristics bitmask (7.44.1).

## Files

- **Create** `libsmbios/include/smbios/tables/tpm_device_entry.hpp` /
  `.cpp`:
  - 0x04 Vendor ID (4 raw ASCII BYTEs - **not** a DMI string index, see
    this story's own Key constraints note)
  - 0x08 Major Spec Version (BYTE)
  - 0x09 Minor Spec Version (BYTE)
  - 0x0A Firmware Version 1 (DWORD - meaning depends on Major Spec
    Version; see Description column)
  - 0x0E Firmware Version 2 (DWORD - 0 for TPM 1.2; least-significant 32
    bits of a vendor-specific value for TPM 2.0)
  - 0x12 Description (STRING - this one genuinely is a DMI string index,
    unlike Vendor ID at 0x04)
  - 0x13 Characteristics (QWORD, bit-field - see 7.44.1)
  - 0x1B OEM-defined (DWORD)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `TpmDeviceEntry` - new. `get_vendor_id_string()` (reads the 4 raw bytes
  at 0x04 and renders as ASCII, trimming a trailing `0x00` per the
  spec's own 3-character example - document explicitly in this
  accessor's own Doxygen comment that these are raw bytes, not a
  `dmi_string()` call), `get_major_spec_version()`,
  `get_minor_spec_version()`, `get_firmware_version_1()`,
  `get_firmware_version_2()`, `get_description_string()` (this one *is*
  `dmi_string()`-backed), `get_characteristics()` +
  `bitset_to_properties<uint64_t>()`-based
  `get_characteristics_string()` (7.44.1's 6 defined/reserved bits, bits
  6:63 reserved), `get_oem_defined()`.

## Constraints

- `get_vendor_id_string()` must not call `dmi_string()` - it reads 4
  bytes directly via `read_u8` x4 (or a single `read_u32` reinterpreted
  byte-by-byte) from `raw_data()` at offset 0x04, independent of this
  entry's own DMI string table.
- `bitset_to_properties<T>()`'s existing template (in
  `AbstractSMBiosEntry`) is already generic over integral `T` - confirm
  it instantiates cleanly at `uint64_t` for this QWORD bitmask (every
  existing call site in this codebase uses `uint8_t`; this is the first
  QWORD-width bitmask in the project) before assuming no changes are
  needed to the shared template itself.

## Tests

- `tests/unit/tpm_device_entry_test.cpp` (new): a TPM 1.2-shaped buffer
  (Major Spec Version 0x01), a TPM 2.0-shaped buffer (Major Spec Version
  0x02), a 3-character Vendor ID ("ABC" with trailing `0x00`, verifying
  correct trimming) and a 4-character Vendor ID ("ABCD", verifying no
  over-trimming), and at least one Characteristics bit set.

## Success criteria

- [x] Registers and resolves through the factory for `SMBiosHandler ==
      43`.
- [x] Vendor ID 3-char and 4-char cases both verified distinctly.
- [x] `bitset_to_properties<uint64_t>()` confirmed working (compiles and
      renders correctly) for this QWORD field.
- [x] `smbios` CLI displays the type on real hardware where present (via
      the existing generic factory-dispatch loop in `main.cpp` - no
      per-type code needed, same as every type since milestone 0005).
