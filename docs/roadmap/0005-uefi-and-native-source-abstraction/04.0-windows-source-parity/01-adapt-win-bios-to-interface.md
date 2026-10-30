# 01 - Adapt win_bios to interface

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Give the existing Windows native firmware-table source an `ISmbiosSource`
face, with the underlying API calls in `win_native_api_helper.h`
untouched.

## Files

- **Modify** `smbios/include/smbios/win_bios.h` / `smbios/src/win_bios.cpp`:
  add `: public ISmbiosSource` to `SMBiosImpl` (or introduce
  `WindowsNativeSource` as a thin delegating wrapper - decide based on how
  cleanly `SMBiosImpl`'s existing `smbios_read_success()`/
  `get_table_base()`/`get_table_size()`/`get_major_version()`/
  `get_minor_version()` map onto `is_available()`/`try_read()`'s shape;
  record the decision in `status.md`).
- **Modify** `smbios/include/smbios/smbios_source_chain.h` / `.cpp`:
  Windows branch of `make_default_source_chain()` returns
  `{WindowsNativeSource-or-adapted-SMBiosImpl, LegacyMemoryScanSource}`.

## Functions and Classes

- `WindowsNativeSource` (or adapted `SMBiosImpl`) - implements
  `ISmbiosSource`; no new native API usage.

## Constraints

- This task must produce a **zero-diff** result at the Windows API call
  level - the only acceptable changes are the new interface's method
  names/signatures wrapping the existing logic, not any change to which
  Windows APIs are called or how.

## Tests

- No new test beyond confirming the adaptation compiles and the existing
  Windows functional test suite passes unmodified.

## Success criteria

- [ ] `smbios` builds on Windows with `WindowsNativeSource` (or the
      adapted `SMBiosImpl`) implementing `ISmbiosSource`.
- [ ] No Windows API call site changed.
