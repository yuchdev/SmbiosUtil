# 02 - Fix unix get_table_size and stub honesty

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add the missing `return` in `SMBiosImpl::get_table_size()`
([unix_bios.cpp:51-55](/smbios/src/unix_bios.cpp)), and correct the doc
comments in `unix_bios.h`/`unix_bios.cpp` that currently claim broader
`*nix` support than the implementation provides, so the gap between
"documented" and "actual" behavior is zero going into milestone 0005 (which
is where the gap actually gets closed with real implementations).

## Files

- **Modify** `smbios/src/unix_bios.cpp`:
  ```cpp
  size_t SMBiosImpl::get_table_size() const
  {
      return table_buffer_.size();
  }
  ```
- **Modify** `smbios/src/unix_bios.cpp`'s `sysfs_table_exists()`: remove the
  `#if 1 / return false / #else / ... #endif` split (dead code behind a
  compile-time-constant branch is itself a code smell `-Wunreachable-code`
  or a linter would flag); keep the single `return false;` with a comment
  stating plainly that sysfs-based SMBIOS discovery is not yet implemented
  and is milestone 0005's Story 02.0.
- **Modify** `smbios/include/smbios/unix_bios.h`'s class doc comment: remove
  "and has since been reported to work on FreeBSD, NetBSD, OpenBSD, BeOS
  and Solaris" (unverifiable and, as shown by `sysfs_table_exists()`
  returning `false` unconditionally, not currently true for any of them
  via the native path - they all fall through to the physical-memory scan
  exactly as Linux does today). State the real, current behavior: "Falls
  through to the legacy physical-memory scan (`SMBios::scan_physical_memory`)
  on every `*nix` platform until milestone 0005 implements a native source."

## Functions and Classes

- `SMBiosImpl::get_table_size()` - fixed.
- `SMBiosImpl::sysfs_table_exists()` - dead-code branch removed, comment
  corrected.

## Constraints

- This task does not implement `reading_from_sysfs()`/`reading_from_efi()`
  - those stay `// TODO` stubs; only the *documentation* of their state
  changes, plus the one real bug fix (`get_table_size()`). Implementing
  them is milestone 0005.
- Under `-Wreturn-type` as `-Werror` (Story 01.0), the current code would
  already fail to build - this task's fix is required for the C++20 raise
  to even compile clean, so land it (or verify Story 01.0 lands it as a
  forced fix) before closing Story 01.0.

## Success criteria

- [ ] `get_table_size()` returns the actual buffer size (verified by a new
      test constructing a `SMBiosImpl` against a synthetic buffer and
      asserting the returned size matches).
- [ ] `unix_bios.h`'s class doc comment makes no claim this milestone
      cannot verify true today.
- [ ] `-Wreturn-type` no longer fires on this function.
