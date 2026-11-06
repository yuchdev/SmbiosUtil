# Story 02.0 - Linux sysfs & EFI Sources

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`ISmbiosSource`); Story 01.1 (the
`smbios_source_linux` library skeleton this story's files land inside -
see [01.1's README](../01.1-per-os-source-libraries/README.md))

## Scope

Implement two real `ISmbiosSource`s for Linux: a sysfs-based one reading
`/sys/firmware/dmi/tables/{smbios_entry_point,DMI}` directly (the modern,
preferred mechanism on any Linux kernel exposing it, no raw `/dev/mem`
access needed), and a UEFI-systab-driven one that actually reads the table
(not just detects EFI's presence, which is all `efi_table_exists()` does
today) for kernels/configurations where the sysfs table files are absent
but EFI is in use. Both are placed ahead of `LegacyMemoryScanSource` in
Linux's chain.

Both are **direct ports**, not re-derivations: this project's own
`c-versions/dmidecode_linux` reference confirms the exact paths
(`SYS_ENTRY_FILE`/`SYS_TABLE_FILE` in `dmidecode.c`) and the EFI systab
line format (`address_from_efi()`'s `SMBIOS=`/`SMBIOS3=` parsing) - see
`plan.md`'s "Reference material" section.

## Tasks

| #  | Document                                      | Status         | Blocks |
|----|--------------------------------------------------|----------------|--------|
| 01 | [sysfs source](01-sysfs-source.md)                 | ⬜ Not started | 03     |
| 02 | [EFI systab source](02-efi-systab-source.md)       | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                               | ⬜ Not started | -      |

## Key constraints

- Both sources require only standard file-read permissions on
  `/sys/firmware/...` paths - whether that needs root depends on the
  distribution's sysfs permission defaults (some restrict
  `smbios_entry_point`/`DMI` to root, some do not); each source's
  `is_available()` must check actual read permission, not just path
  existence, so a permission-denied case correctly falls through to the
  next source in the chain rather than reporting false availability.
- `unix_bios.h`'s existing class doc comment (corrected in milestone
  0002's Story 02.0, Task 02 to state the real, current behavior) is
  updated again here to describe what is now actually implemented.

## Files or directories created or modified by this story

- `smbios/include/smbios/source_linux/linux_sysfs_source.h` /
  `smbios/src/source_linux/linux_sysfs_source.cpp` (new; path updated per
  Story 01.1's `smbios_source_linux` library - was planned as a flat
  `smbios/include/smbios/` path before that story landed).
- `smbios/include/smbios/source_linux/linux_efi_source.h` /
  `smbios/src/source_linux/linux_efi_source.cpp` (new; same path update).
- `smbios/include/smbios/source_linux/unix_bios.h` /
  `smbios/src/source_linux/unix_bios.cpp` - the old `SMBiosImpl` class
  (per-platform PImpl, pre-dating `ISmbiosSource`, relocated here verbatim
  by Story 01.1) is retired once both new sources are registered in
  `smbios_source_linux`'s own `make_default_source_chain()` definition;
  confirm BSD/Solaris targets (which, per Story 01.1, get no per-OS
  library and link `smbios_source_legacy` directly) fall through cleanly
  to `LegacyMemoryScanSource` only, since this story implements Linux
  specifically, not every `*nix` CLAUDE.md's build docs mention.
- `smbios/src/source_linux/CMakeLists.txt` - add the two new `.cpp` files
  to the library Story 01.1 already created.
