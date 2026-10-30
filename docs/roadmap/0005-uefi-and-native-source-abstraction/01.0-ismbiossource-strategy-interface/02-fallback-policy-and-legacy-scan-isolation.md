# 02 - Fallback policy and legacy-scan isolation

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Move `SMBios::scan_physical_memory()`, `extract_dmi_version()`,
`checksum32()`/`checksum64()`/`smbios_crc()`, and the
`SMBIOSEntryPoint32`/`SMBIOSEntryPoint64` structs + `smbios_anchor.h`'s
`detect_smbios_anchor()` usage out of `SMBios` itself and into a new
`LegacyMemoryScanSource : public ISmbiosSource`, so the legacy scan is an
ordinary, swappable, last-priority chain entry (R3) rather than logic
embedded directly in `SMBios`'s constructor. Also add the shared
version-fixup helper **R9** calls for, since this is where entry-point
version extraction already lives after the move.

## Files

- **Create** `smbios/include/smbios/legacy_memory_scan_source.h` / `.cpp`:
  `LegacyMemoryScanSource::is_available()` returns `true` unconditionally
  (it is only ever reached as the last resort, so it always "tries"); its
  `try_read()` contains exactly today's `scan_physical_memory` +
  `extract_dmi_version` + checksum-validated base/length extraction logic,
  moved verbatim where possible.
- **Modify** `smbios/include/smbios/smbios.h` / `smbios/src/smbios.cpp`:
  remove the moved members (`smbios_entry32_`, `smbios_entry64_`,
  `checksum_validated_`, `devmem_base_`, `devmem_length_`, `entry_point_buffer_`,
  the `scan_physical_memory`/`extract_dmi_version`/`checksum32`/`checksum64`/
  `smbios_crc` method declarations) from `SMBios` once they live in
  `LegacyMemoryScanSource`; `render_to_description()`'s two branches
  (32-bit/64-bit entry point dump) move with them, exposed via a new
  `ISmbiosSource::render_diagnostic_description()` optional method or kept
  as a `LegacyMemoryScanSource`-specific accessor `SMBios::render_to_description()`
  delegates to when the legacy source was the one that succeeded (exact
  shape: resolve during implementation, record the decision in `status.md`).
- **Create** `smbios/include/smbios/smbios_version_fixup.h` (header-only):
  ```cpp
  // Ported from c-versions/dmidecode_linux/dmidecode.c::dmi_fixup_version()
  // "Some BIOS report weird SMBIOS version, fix that up."
  inline void fixup_smbios_version(size_t& major, size_t& minor) {
      const uint16_t packed = static_cast<uint16_t>((major << 8) | minor);
      switch (packed) {
          case 0x021F: case 0x0221: major = 2; minor = 3; break; // 2.31/2.33 -> 2.3
          case 0x0233: major = 2; minor = 6; break;              // 2.51 -> 2.6
          default: break;
      }
  }
  ```
  (exact malformed-value table re-verified against the reference during
  implementation, not copied from this summary alone - the reference's
  own comments reference specific real-world BIOS quirks worth preserving
  verbatim).

## Functions and Classes

- `smbios::LegacyMemoryScanSource` - new, implementing `ISmbiosSource`.
- `smbios::fixup_smbios_version(size_t&, size_t&)` - new, R9.
- `SMBios` - loses the members/methods listed above; gains a reference to
  whichever source actually succeeded, for diagnostics.

## Constraints

- `make_default_source_chain()` (Task 01) places
  `LegacyMemoryScanSource` **last** in every platform's chain, never
  first - this is the concrete enforcement of R3.
- `render_to_description()`'s existing two-branch format (32-bit vs.
  64-bit entry point) must still be reachable and byte-identical when the
  legacy source is what succeeded; it is simply unreachable (returns an
  empty string, as today's code already does when neither entry-point
  pointer is set) when a native source succeeded instead.
- `fixup_smbios_version()` is called by `LegacyMemoryScanSource` here,
  and is reused as-is (not reimplemented) by every other `ISmbiosSource`
  that decodes a raw entry point - Story 02.0's Linux sources and Story
  03.0's macOS source, Story 04.0's Windows source only if its native API
  can itself report one of the same malformed values (confirm during that
  story; the Windows firmware-table API may already normalize this).

## Tests

- `tests/unit/legacy_memory_scan_source_test.cpp` (new): exercises
  `is_available()`/`try_read()` against the existing functional test's
  real-hardware fallback path (on a machine/CI job where no native source
  is implemented yet, i.e. every `*nix` target until Story 02.0/03.0
  land), confirming byte-identical output to pre-refactor `SMBios`.
- `tests/unit/smbios_version_fixup_test.cpp` (new): each malformed input
  from the reference's table maps to its documented corrected value; an
  already-valid version is passed through unchanged.

## Success criteria

- [ ] `SMBios` no longer declares `scan_physical_memory`/
      `extract_dmi_version`/`checksum32`/`checksum64`/`smbios_crc`.
- [ ] `LegacyMemoryScanSource` reproduces today's fallback behavior exactly
      on real hardware (Linux/macOS, where it is still the only thing that
      runs until later stories land).
- [ ] `render_to_description()`'s legacy-entry-point dump is unchanged
      when the legacy source succeeds.
- [ ] `fixup_smbios_version()` matches the reference's correction table
      exactly, verified by test.
