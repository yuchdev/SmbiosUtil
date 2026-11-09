# Story 04.0 - Windows Source Parity

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete (2026-11-07)
**Depends on:** Story 01.0 (`ISmbiosSource`); Story 01.1 (the
`smbios_source_windows` library this story's adapted class replaces the
temporary chain-factory stub inside - see
[01.1's README](../01.1-per-os-source-libraries/README.md))

## Scope

Adapt the existing Windows native firmware-table API source
(`win_bios.hpp`/`win_bios.cpp`/`win_native_api_helper.hpp`) to implement
`ISmbiosSource` directly, with **zero behavior change** (R6) - this story
exists only so Windows participates in the same runtime chain/diagnostics
model (R1-R3) the other platforms now use, not to touch any of its actual
native-API logic.

## Tasks

| #  | Document                                                      | Status         | Blocks |
|----|--------------------------------------------------------------------|----------------|--------|
| 01 | [Adapt win_bios to interface](01-adapt-win-bios-to-interface.md)     | ✅ Complete - ratified keeping WindowsNativeSource as the delegating wrapper, see status.md | 02     |
| 02 | [Tests](02-tests.md)                                                | ✅ Complete - real Windows CI, see status.md | -      |

## Key constraints

- No change to `win_native_api_helper.hpp`'s actual Windows API calls.
- `make_default_source_chain()`'s Windows branch becomes
  `{WindowsNativeSource, LegacyMemoryScanSource}` - the legacy scan stays
  as a last resort on Windows too (R3), matching every other platform's
  chain shape even though the native source is expected to succeed in
  the overwhelming majority of real-world cases.

## Files or directories created or modified by this story

- `smbios/include/smbios/source_windows/win_bios.hpp` /
  `smbios/src/source_windows/win_bios.cpp` (path updated per Story 01.1's
  `smbios_source_windows` library - was planned as a flat
  `smbios/include/smbios/` path before that story landed) - `SMBiosImpl`
  either renamed/repurposed as `WindowsNativeSource` implementing
  `ISmbiosSource`, or wrapped by a thin new `WindowsNativeSource` class
  delegating to the unchanged `SMBiosImpl` - whichever keeps the diff
  smallest is acceptable; record the choice in `status.md`. This also
  replaces Story 01.1's temporary inline adapter in this same library
  with the real, permanent `ISmbiosSource` implementation.
