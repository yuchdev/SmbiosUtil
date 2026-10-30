# 01 - sysfs source

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `LinuxSysfsSource : public ISmbiosSource` reading
`/sys/firmware/dmi/tables/smbios_entry_point` (the entry point - gives
major/minor version and table length) and
`/sys/firmware/dmi/tables/DMI` (the raw structure table itself) - the
mechanism `unix_bios.cpp`'s `sysfs_table_exists()` comment already named
as the TODO, now actually implemented and confirmed byte-for-byte against
`c-versions/dmidecode_linux/dmidecode.c`'s `SYS_ENTRY_FILE`/`SYS_TABLE_FILE`
constants and its "First try reading from sysfs tables" block in `main()`.

## Files

- **Create** `smbios/include/smbios/linux_sysfs_source.h` / `.cpp`:
  ```cpp
  class LinuxSysfsSource : public ISmbiosSource {
  public:
      bool is_available() const override; // both files exist AND are readable
      bool try_read(std::vector<uint8_t>& out_table, size_t& major, size_t& minor) const override;
      const char* source_name() const override { return "linux-sysfs"; }
  };
  ```
  `is_available()` opens both files read-only and closes them immediately
  (a permission probe, not a full read) rather than only checking
  `std::filesystem::exists` (which would report a file "available" even
  if unreadable by the current user - the exact edge case this task's
  parent story's Key constraints call out). `try_read()` parses the entry
  point's anchor/checksum/version/length fields using milestone 0003's
  `read_uN` accessors (the entry point file's own bytes are the same wire
  format `SMBIOSEntryPoint32`/`64` already modeled, just now read without
  a struct overlay), then reads `DMI` in full as the raw table.

## Functions and Classes

- `smbios::LinuxSysfsSource` - new.

## Constraints

- Entry-point parsing in this source **reuses** milestone 0003's
  `read_uN` accessors directly on the file's bytes - it does not
  reintroduce `reinterpret_cast` onto `SMBIOSEntryPoint32`/`64` just
  because this is new, unrelated-looking code; the anchor/checksum
  validation logic (`_SM_`/`_SM3_`/`_DMI_` detection via
  `smbios_anchor.h`'s existing `detect_smbios_anchor()`) is reused as-is.
- Decoded major/minor are passed through Story 01.0 Task 02's
  `fixup_smbios_version()` (R9) before being written to the
  `try_read()` out-parameters - the reference applies this fixup
  immediately after decoding the entry point, in every call path, and
  this source must match that.
- No new runtime dependency - plain `<fstream>`/POSIX file I/O.

## Tests

- `tests/unit/linux_sysfs_source_test.cpp` (new, Linux-only): against a
  temp-directory fixture mimicking `/sys/firmware/dmi/tables/` layout
  (both files present and readable; one missing; one present but
  unreadable via `chmod 000`, run as non-root) - confirm `is_available()`
  and `try_read()` behave correctly in each case. Real-hardware
  verification against the actual `/sys/firmware/dmi/tables/` path is a
  separate, manually-run check (CI likely runs as a user without
  permission to read it, which is itself a valid "unavailable, fall
  through" test case, not a failure).

## Success criteria

- [ ] On a Linux machine/container where `/sys/firmware/dmi/tables/` is
      root-readable, running as root produces identical parsed output to
      the legacy scan on the same machine.
- [ ] Running as non-root on a machine where the sysfs files are
      root-only correctly falls through to the next source in the chain
      (verified by `source_name()` reporting something other than
      `"linux-sysfs"` in that case).
