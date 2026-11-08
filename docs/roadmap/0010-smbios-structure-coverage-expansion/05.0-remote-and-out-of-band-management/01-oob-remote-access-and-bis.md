# 01 - Out-of-Band Remote Access and BIS Entry Point

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `OutOfBandRemoteAccessEntry` (`SMBiosHandler` value `30`),
DSP0134 3.9.0 section 7.31, and `BootIntegrityServicesEntry` (value `31`),
section 7.32 (content-free per spec - see this story's own Key
constraints and status.md's note).

## Files

- **Create**
  `libsmbios/include/smbios/tables/out_of_band_remote_access_entry.h` /
  `.cpp`:
  - 0x04 Manufacturer Name (STRING)
  - 0x05 Connections (BYTE, bit-field - this task spec's own source
    excerpt redacted this row; confirm its exact bit layout against
    DSP0134 3.9.0's full Table 103 directly before implementing - known
    from the prose summary to cover at least "Outbound Connection
    Enabled" and "Inbound Connection Enabled" bits)
- **Create** `libsmbios/include/smbios/tables/boot_integrity_services_entry.h` /
  `.cpp`: no decoded fields - constructor performs the usual type check,
  `render_to_description()` states this structure is reserved for the
  external Boot Integrity Services API and carries no DSP0134-defined
  fields.
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for both.

## Functions and Classes

- `OutOfBandRemoteAccessEntry` - new. `get_manufacturer_name_string()`,
  `get_connections()` + decoded accessors per the confirmed bit layout
  (e.g. `is_outbound_connection_enabled()`,
  `is_inbound_connection_enabled()` - exact names pending spec-table
  confirmation).
- `BootIntegrityServicesEntry` - new, minimal: inherits
  `get_entry_size()` from `AbstractSMBiosEntry` unchanged, overrides only
  `get_type()` ("Boot Integrity Services Entry Point") and
  `render_to_description()`.

## Constraints

- `OutOfBandRemoteAccessEntry`'s Connections bit layout must be confirmed
  against the full spec table before implementation (same redaction
  caveat as Story 04.0 Task 02's System Reset/Hardware Security fields -
  this is the fourth instance of the same source-extraction gap in this
  milestone's spec research; each affected task independently confirms
  its own field against the real document rather than propagating a
  guess from one task to another).
- `BootIntegrityServicesEntry` must not invent fields the spec does not
  define - resist the temptation to reverse-engineer the external BIS API
  spec's own structure layout; DSP0134 3.9.0 itself defines none, so
  neither does this class.

## Tests

- `tests/unit/out_of_band_remote_access_entry_test.cpp`,
  `tests/unit/boot_integrity_services_entry_test.cpp` (new): a
  minimum-length buffer plus a Connections-bits-set buffer for the first;
  a single minimal-buffer registration/resolution case for the second
  (nothing else to vary, since it has no fields).

## Success criteria

- [x] Connections bit layout confirmed against the full spec table: Bit 0
      Inbound Connection Enabled, Bit 1 Outbound Connection Enabled, Bits
      7:2 Reserved (DSP0134 3.9.0 7.31, Table 103).
- [x] Both types register and resolve through the factory
      (`SMBiosHandler` values 30 and 31).
- [x] `smbios` CLI displays both types on real hardware where present (via
      the existing generic factory-dispatch loop in `main.cpp`; the
      CLI-linking bug tracked in `task_dc27f15d` is unchanged by this
      task).
