# Story 03.0 - macOS UEFI Source

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`ISmbiosSource`)

## Scope

Implement `MacOsUefiSource : public ISmbiosSource`, the first native
SMBIOS source macOS has ever had in this project - closing the gap
CLAUDE.md names directly ("Mac has no native SMBIOS source"). Ports the
IOKit mechanism confirmed in this project's own
`c-versions/dmidecode_macos` reference (the Acidanthera dmidecode fork) -
`util.c`'s `mem_chunk_ioreg()` - rather than `/dev/mem` (not generally
usable for this purpose on modern macOS).

## Tasks

| #  | Document                                                  | Status         | Blocks |
|----|--------------------------------------------------------------|----------------|--------|
| 01 | [macOS IORegistry reader](01-macos-efi-nvram-reader.md)       | ⬜ Not started | 02     |
| 02 | [macOS SMBIOS source](02-macos-smbios-source.md)              | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                         | ⬜ Not started | -      |

(Task 01's filename is kept as `01-macos-efi-nvram-reader.md` for roadmap
continuity with earlier links into it, even though its content now
describes the confirmed IORegistry mechanism rather than NVRAM/EFI
variables - do not rename the file without updating every inbound link via
`/link-check`.)

## Key constraints

- **The mechanism is confirmed, not assumed** (see `status.md`): look up
  `IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS` via
  `IORegistryEntryFromPath`, read its `"SMBIOS-EPS"` CFData property for
  the entry point and its `"SMBIOS"` property for the raw table. Task 01
  ports this directly from the reference; it is a focused implementation
  task, not an exploratory spike.
- **Deliberate deviation from the reference** (recorded in `plan.md`'s
  "Reference material" section): the reference reaches this code path
  only through a call site gated `#if defined __i386__ || defined
  __x86_64__`, which would skip it entirely on a native Apple Silicon
  build. `MacOsUefiSource` calls the IORegistry path unconditionally on
  `__APPLE__`, independent of CPU architecture - IOKit property reads do
  not depend on architecture, and native Apple Silicon support is this
  story's goal, not an accident of the reference's historical gating.
- IOKit (`<IOKit/IOKitLib.h>`) and CoreFoundation are new, macOS-only
  dependencies - linked only when building on `__APPLE__`, via
  `find_library` in CMake, not a vcpkg package (both are system
  frameworks).
- No `/dev/mem`/root requirement is introduced by this source (R7) - this
  should be the first SMBIOS source in this entire project that works
  for a non-root user, worth highlighting in the milestone's documentation
  touch-list.

## Files or directories created or modified by this story

- `smbios/include/smbios/macos_efi_reader.h` / `.cpp` (new) - low-level
  IORegistry property read, ported from `mem_chunk_ioreg()`.
- `smbios/include/smbios/macos_uefi_source.h` / `.cpp` (new) - the
  `ISmbiosSource` implementation.
- Root `CMakeLists.txt` / `smbios/CMakeLists.txt` - link `IOKit`/
  `CoreFoundation` frameworks on `__APPLE__`.
- `c-versions/dmidecode_macos` - read as a reference during this story;
  never modified or built from (CLAUDE.md: untracked reference material,
  not part of the build).
