# 02 - EFI systab source

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `LinuxEfiSource : public ISmbiosSource`, completing what
`unix_bios.cpp`'s `efi_table_exists()`/`reading_from_efi()` only half-did
today - `efi_table_exists()` already greps `/sys/firmware/efi/systab` or
`/proc/efi/systab` for an `SMBIOS=`/`SMBIOS3=` line and returns whether one
exists, but `reading_from_efi()` is an empty body that never actually maps
and reads the physical address that line names. The line format and
parsing logic are confirmed against
`c-versions/dmidecode_linux/dmidecode.c`'s `address_from_efi()` - port its
`strchr(linebuf, '=')` / `strcmp(linebuf, "SMBIOS3")` / `strcmp(linebuf,
"SMBIOS")` logic directly rather than re-deriving the line format.

## Files

- **Create** `smbios/include/smbios/linux_efi_source.h` / `.cpp`:
  ```cpp
  class LinuxEfiSource : public ISmbiosSource {
  public:
      bool is_available() const override; // systab exists and names SMBIOS=/SMBIOS3=
      bool try_read(std::vector<uint8_t>& out_table, size_t& major, size_t& minor) const override;
      const char* source_name() const override { return "linux-efi-systab"; }
  };
  ```
  `is_available()` reuses `unix_bios.cpp`'s existing `systab` line-grep
  logic for both `filename1`/`filename2` paths. `try_read()` parses the
  physical address out of the matched `SMBIOS=0x...`/`SMBIOS3=0x...` line,
  maps it via `smbios::PhysicalMemory` (the existing, already-portable
  `/dev/mem`-mapping class - unchanged by this task), and decodes the
  entry point there using the same `read_uN`-based logic as Task 01's
  sysfs source, then maps and reads the table it points to.

## Functions and Classes

- `smbios::LinuxEfiSource` - new.
- Reuses `smbios::PhysicalMemory` (unchanged) and `smbios::detect_smbios_anchor`
  (unchanged) from the existing codebase.

## Constraints

- This source still needs `/dev/mem` access (it maps a physical address
  the way the legacy scan does) - it is **not** a privilege-free
  alternative to the legacy scan the way Task 01's sysfs source is; its
  value over the legacy scan is correctness (it reads the address the
  firmware itself published via EFI, rather than blindly scanning the
  0xF0000-0xFFFFF legacy BIOS shadow region, which may not even be mapped
  meaningfully on a UEFI-only boot).
- Placed in the chain **after** `LinuxSysfsSource` (Task 01) and
  **before** `LegacyMemoryScanSource`, per the milestone's stated priority
  order (sysfs → EFI → legacy scan) - matching the reference's own
  dispatch order in `main()` (sysfs block, then `address_from_efi()`, then
  the `memory_scan:` label).
- Decoded major/minor are passed through `fixup_smbios_version()` (R9,
  Story 01.0 Task 02) before being written to the `try_read()`
  out-parameters, same as Task 01.

## Tests

- `tests/unit/linux_efi_source_test.cpp` (new): a synthetic `systab` file
  fixture with a known `SMBIOS=0x...` line, confirming address parsing;
  full read-path verification is a real-hardware manual check (needs
  `/dev/mem` access) since no portable fixture can substitute for an
  actual physical-memory mapping.

## Success criteria

- [ ] `is_available()` correctly detects both `SMBIOS=` (32-bit) and
      `SMBIOS3=` (64-bit) systab lines.
- [ ] On a real UEFI Linux machine with `/dev/mem` access, `try_read()`
      produces output matching the legacy scan's output on the same
      machine (cross-validation that the EFI-published address and the
      shadow-ROM scan agree, where both are available).
