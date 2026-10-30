# Story 04.0 - Windows Source Parity

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`ISmbiosSource`)

## Scope

Adapt the existing Windows native firmware-table API source
(`win_bios.h`/`win_bios.cpp`/`win_native_api_helper.h`) to implement
`ISmbiosSource` directly, with **zero behavior change** (R6) - this story
exists only so Windows participates in the same runtime chain/diagnostics
model (R1-R3) the other platforms now use, not to touch any of its actual
native-API logic.

## Tasks

| #  | Document                                                      | Status         | Blocks |
|----|--------------------------------------------------------------------|----------------|--------|
| 01 | [Adapt win_bios to interface](01-adapt-win-bios-to-interface.md)     | ⬜ Not started | 02     |
| 02 | [Tests](02-tests.md)                                                | ⬜ Not started | -      |

## Key constraints

- No change to `win_native_api_helper.h`'s actual Windows API calls.
- `make_default_source_chain()`'s Windows branch becomes
  `{WindowsNativeSource, LegacyMemoryScanSource}` - the legacy scan stays
  as a last resort on Windows too (R3), matching every other platform's
  chain shape even though the native source is expected to succeed in
  the overwhelming majority of real-world cases.

## Files or directories created or modified by this story

- `smbios/include/smbios/win_bios.h` / `smbios/src/win_bios.cpp` -
  `SMBiosImpl` either renamed/repurposed as `WindowsNativeSource`
  implementing `ISmbiosSource`, or wrapped by a thin new
  `WindowsNativeSource` class delegating to the unchanged `SMBiosImpl` -
  whichever keeps the diff smallest is acceptable; record the choice in
  `status.md`.
