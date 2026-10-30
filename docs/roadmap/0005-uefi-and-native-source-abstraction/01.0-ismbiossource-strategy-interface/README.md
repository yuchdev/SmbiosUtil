# Story 01.0 - ISmbiosSource Strategy Interface

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Define `ISmbiosSource` (contract **C1**), rewrite `SMBios`'s constructor
to build and walk an ordered source chain (contract **C2**) instead of
calling one compile-time-selected `SMBiosImpl`, and wrap the existing
legacy physical-memory scan (`scan_physical_memory`/`extract_dmi_version`/
`checksum32`/`checksum64`) as the chain's explicit last-resort
`ISmbiosSource` implementation - with zero behavior change versus today,
since no new source exists yet; this story only changes the *shape* the
later stories' real sources plug into. Also adds a shared
`fixup_smbios_version()` helper (R9), ported from
`c-versions/dmidecode_linux`'s `dmi_fixup_version()`, for every later
story's source to reuse when decoding a raw entry point's version bytes.

## Tasks

| #  | Document                                                                | Status         | Blocks |
|----|------------------------------------------------------------------------------|----------------|--------|
| 01 | [Define interface and chain](01-define-interface-and-chain.md)              | ⬜ Not started | 02     |
| 02 | [Fallback policy and legacy-scan isolation](02-fallback-policy-and-legacy-scan-isolation.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                        | ⬜ Not started | -      |

## Key constraints

- `SMBios`'s public interface (`get_smbios_version()`,
  `get_structures_count()`, `get_table_base()`, `get_table_size()`,
  `render_to_description()`, the iterator) does not change in this story -
  only its *constructor's* internals change.
- This story alone must not change behavior on any platform (R3's
  "fallback becomes explicit, not accidental" still means the fallback
  *runs* exactly when it runs today) - Stories 02.0-04.0 are what actually
  add new, real sources; this story only builds the structure they plug
  into.

## Files or directories created or modified by this story

- `smbios/include/smbios/ismbios_source.h` (new).
- `smbios/include/smbios/smbios.h` / `smbios/src/smbios.cpp` - constructor
  rewritten around the source chain; `scan_physical_memory`/
  `extract_dmi_version`/`checksum32`/`checksum64` and the
  `SMBIOSEntryPoint32`/`64` structs move into a new `LegacyMemoryScanSource`
  implementation (file location to be decided during Task 01 -
  likely `smbios/src/legacy_memory_scan_source.cpp`).
- `smbios/include/smbios/smbios_version_fixup.h` (new, Task 02).
- `smbios/include/smbios/win_bios.h`/`unix_bios.h` - unaffected by this
  story; Story 04.0 and Story 02.0/03.0 adapt them to the new interface.
