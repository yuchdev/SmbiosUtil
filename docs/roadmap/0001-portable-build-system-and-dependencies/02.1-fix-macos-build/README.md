# Story 02.1 - Fix macOS Build

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (its CI matrix surfaced this; its `SMBIOS_*` CMake variables are what Task 01 edits)

## Scope

Make `smbios`/`smbios_util` actually compile and link on macOS, fixing
the two independent root causes confirmed by local, real compilation
(see milestone `status.md` for the exact `clang++` commands and output):
the root `CMakeLists.txt`'s compiler-selection branch never matching
`AppleClang`, and three `#if` guards (`unix_bios.h`, `unix_bios.cpp`,
`posix_physical_memory.h`, `posix_physical_memory.cpp`,
`physical_memory.cpp`) excluding `__APPLE__` entirely, leaving
`SMBiosImpl`/`NativePhysicalMemory` with no definition at all on that
platform.

This is a scoped, user-ratified exception to the milestone's own
"no source changes" rule (`plan.md`) - it fixes exactly these two things,
nothing else. It does **not** do milestone 0002's full C++20/`-Werror`
raise, and it does **not** give macOS a working native SMBIOS source
(still milestone 0005's `MacOsUefiSource`) - see each task's own
Non-goals/Constraints below for the precise line each one draws.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [Fix compiler-ID branch and standard](01-fix-compiler-id-branch-and-standard.md) | ⬜ Not started | 02     |
| 02 | [Extend physical-memory guards to __APPLE__](02-extend-physical-memory-guards-to-apple.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                              | ⬜ Not started | -      |

## Key constraints

- **"Compiles and links" is this story's whole mandate - not "works."**
  Once fixed, `SMBios`'s only path on macOS is the legacy physical-memory
  scan (`unix_bios.cpp`'s `sysfs_table_exists()`/`efi_table_exists()`
  always return `false` there - no `/sys/firmware/...` on macOS - and
  `/dev/mem` access is expected to fail at runtime on modern macOS due to
  SIP, with or without root). A clean, non-crashing failure (an empty
  table / a thrown, caught exception at the `SMBios` call site) is the
  correct, expected runtime behavior on this story's fix alone - **not**
  a bug this story needs to chase. Milestone 0005's `MacOsUefiSource` is
  what makes macOS actually produce real SMBIOS data; conflating the two
  would reopen a redesign this story was explicitly scoped to avoid.
- Every change is verified by real local compilation on macOS before
  being marked done - not assumed from a diff - continuing this
  milestone's own established discipline.

## Files or directories created or modified by this story

- Root `CMakeLists.txt` - compiler-ID branch condition; the Clang
  branch's `SMBIOS_CXX_STANDARD_FLAG` value.
- `smbios/include/smbios/unix_bios.h`, `smbios/src/unix_bios.cpp`,
  `smbios/include/smbios/posix_physical_memory.h`,
  `smbios/src/posix_physical_memory.cpp`, `smbios/src/physical_memory.cpp`
  - `#if`/`#elif` guard conditions only; no logic changes.
