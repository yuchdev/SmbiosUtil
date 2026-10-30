# 02 - macOS SMBIOS source

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (`read_apple_smbios_property`)

## Objective

Implement `MacOsUefiSource : public ISmbiosSource` using Task 01's reader,
following the two-property sequence confirmed in
`c-versions/dmidecode_macos`: read `"SMBIOS-EPS"` for the entry point,
decode it with milestone 0003's `read_uN` accessors to confirm the anchor
(`_SM_`/`_SM3_`) and extract the version, then read `"SMBIOS"` for the raw
structure table.

## Files

- **Create** `smbios/include/smbios/macos_uefi_source.h` / `.cpp`:
  ```cpp
  class MacOsUefiSource : public ISmbiosSource {
  public:
      bool is_available() const override; // "SMBIOS-EPS" property exists and starts with a valid anchor
      bool try_read(std::vector<uint8_t>& out_table, size_t& major, size_t& minor) const override;
      const char* source_name() const override { return "macos-uefi"; }
  };
  ```
  `try_read()`:
  ```cpp
  auto entry_point = read_apple_smbios_property("SMBIOS-EPS", 0x20);
  if (!entry_point) return false;

  // decode anchor + major/minor via read_uN (milestone 0003), same as
  // the Linux sysfs/EFI sources - no macOS-specific byte-decoding here
  // ... apply the version fixup helper (R9, Story 01.0 Task 02) ...

  auto table = read_apple_smbios_property("SMBIOS", /* no reliable length from entry point - see Constraints */ 0x10000);
  if (!table) return false;
  out_table = std::move(*table);
  return true;
  ```
- **Modify** `smbios/include/smbios/smbios_source_chain.h` / `.cpp`:
  `make_default_source_chain()`'s `__APPLE__` branch becomes
  `{MacOsUefiSource, LegacyMemoryScanSource}` (the legacy scan stays as a
  fallback per R3, even though it is very unlikely to find anything
  meaningful on modern Mac hardware - costs nothing to leave it last).

## Functions and Classes

- `smbios::MacOsUefiSource` - new.

## Constraints

- **Unlike the Linux sources, the entry point's own
  `structure_table_address`/`structure_table_length` fields are not used
  to address the second read** - IORegistry ignores whatever address is
  passed and simply returns the `"SMBIOS"` property's full stored blob
  (per the confirmed reference, `mem_chunk_ioreg()` only inspects the
  requested `len` to decide how much of the property to copy, never the
  `base`). The entry point's **length** field is still worth reading (to
  size the read request / sanity-check against what comes back), but the
  **address** field is meaningless for this source and must not be used
  to construct an offset into anything.
- Entry-point and table parsing reuse milestone 0003's `read_uN`
  accessors and `smbios_anchor.h`'s `detect_smbios_anchor()` exactly as
  the Linux sources do - no macOS-specific byte-decoding logic beyond the
  IORegistry retrieval itself (Task 01).
- Apply the version-fixup helper from Story 01.0, Task 02 (R9) after
  decoding major/minor, before constructing `SMBiosVersion` - macOS BIOS/
  firmware can report the same malformed version pairs
  `dmi_fixup_version()` corrects for on Linux.
- Must work unconditionally on `__APPLE__` regardless of `__arm64__` vs.
  `__x86_64__` (this story's Key constraints - the deliberate deviation
  from the reference's x86-only gating).
- `CMakeLists.txt` links `IOKit`/`CoreFoundation` only for `smbios`'s
  macOS build - confirm via the milestone 0001 CI matrix staying green on
  Windows/Linux too.

## Tests

- `tests/unit/macos_uefi_source_test.cpp` (macOS-only): against a
  synthetic blob matching the confirmed format, assert correct
  entry-point/table decoding; full real-hardware verification is a
  manual check on at least one real Mac - the first time this project can
  claim to work on a Mac at all, so this manual check is the milestone's
  actual proof point, not a formality.

## Success criteria

- [ ] On a real Mac (Apple Silicon if available, not just Intel), 
      `smbios_util` produces a complete SMBIOS report for the first time
      in this project's history, with no root/admin privilege required.
- [ ] `source_name()` reports `"macos-uefi"` on that run (not a fallback).
- [ ] README's "Mac has no native SMBIOS source" statement is removed/
      corrected.
