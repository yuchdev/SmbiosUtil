# Story 01.1 - Per-OS Source Libraries

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`ISmbiosSource`, `make_default_source_chain()`,
`fixup_smbios_version()` must exist first)

## Scope

User-requested addition, inserted after this milestone's original four
stories were already numbered (see `plan.md`'s "why this one was added"
note and [ADR 0001](/docs/adr/0001-per-os-smbios-source-libraries.md) for
the full decision record). Restructures the CMake target graph so every
platform's `ISmbiosSource` implementation is packaged as its **own static
library** (contract **C3**), not a class compiled into the single
`smbios` library - and does so as a pure skeleton: **no new source-reading
logic is written in this story**. Every file this story touches either
moves verbatim (zero behavior change, same as milestone 0001's
`02.1-fix-macos-build` precedent for "this story's mandate is narrower
than it sounds") or is a newly-created, currently-empty library target
ready for Stories 02.0/03.0/04.0 to populate with real source classes.

Concretely, this story creates five CMake targets
(`smbios_source_interface`, `smbios_source_legacy`, `smbios_source_windows`,
`smbios_source_macos`, `smbios_source_linux`), relocates the files that
already exist today into the new targets that will own them, and leaves
`smbios` (core) linking exactly one per-OS library selected by a CMake-time
branch instead of `smbios.cpp`'s current C++ `#if defined(_WIN32)`.

## Tasks

| #  | Document                                                                      | Status         | Blocks |
|----|----------------------------------------------------------------------------------|----------------|--------|
| 01 | [CMake target graph and shared interface/legacy libraries](01-cmake-target-graph-and-shared-libraries.md) | ✅ Complete - combined with Task 02, see status.md | 02     |
| 02 | [Relocate existing per-OS files; macOS and Linux skeletons](02-relocate-existing-sources-and-stub-skeletons.md) | ✅ Complete - pulled forward into Task 01's pass (CMakeLists.txt referenced these directories before they existed) | 03     |
| 03 | [Tests](03-tests.md)                                                            | ⬜ Not started | -      |

## Key constraints

- **Skeleton only - no new `ISmbiosSource` subclass gets real logic here.**
  `LinuxSysfsSource`, `LinuxEfiSource`, and `MacOsUefiSource` do not exist
  yet after this story lands; only their libraries' CMake scaffolding does.
  Stories 02.0/03.0 add the actual classes, inside the libraries this
  story already created for them.
- **Zero behavior change on every platform**, same discipline Story 01.0
  itself commits to: Windows must keep using its real native source
  (`win_bios.cpp`'s logic, relocated but not rewritten) - it must **not**
  regress to legacy-scan-only just because this story landed before Story
  04.0's interface adaptation. Linux and macOS, which have no real native
  source today, keep producing exactly what they produce today (legacy
  scan only) through each platform's new library's trivial
  `make_default_source_chain()` definition.
- **Resolves R8 more completely than Story 01.0 Task 01's original plan.**
  That task expected a "minimal remaining `#if`" inside
  `smbios_source_chain.cpp`. This story supersedes that detail: with
  `make_default_source_chain()` *defined* once per per-OS library (a
  link-seam) instead of once per `#if` branch in one file, `smbios.cpp`
  and `smbios_source_chain.cpp` end up with **zero** `#if defined(_WIN32)`
  at all - the platform branch moves entirely into
  `smbios/src/CMakeLists.txt` as a CMake `if()`. Everything else Story
  01.0 Task 01/02 already built (the interface shape, the chain-walking
  loop, the fallback-isolation logic) is unaffected; only *where* each
  piece physically compiles changes.
- `smbios_util` and every existing test target continue depending only on
  `smbios` (core) - this story is invisible to them; `smbios` itself
  absorbs the new per-OS library transitively via its own CMake `if()`
  branch.
- Picks up the open file-location question Story 01.0 Task 02 explicitly
  left unresolved ("file location to be decided during Task 01 - likely
  `smbios/src/legacy_memory_scan_source.cpp`") and resolves it: once
  Story 01.0 lands `LegacyMemoryScanSource` inside core `smbios` (per its
  own, unmodified plan), this story's Task 01 relocates it into the new
  `smbios_source_legacy` library - a packaging move, not a logic change,
  and not an edit to Story 01.0's own files.

## Files or directories created or modified by this story

- **New:** `smbios/src/source_interface/` + `smbios/include/smbios/source_interface/`
  - `smbios_source_chain.h` (moved from Story 01.0's planned location,
    declaration only).
- **New:** `smbios/src/source_legacy/` + `smbios/include/smbios/source_legacy/`
  - `physical_memory.h`/`.cpp`, `posix_physical_memory.h`/`.cpp`,
    `win_physical_memory.h`/`.cpp` (moved verbatim from `smbios/include/smbios/`
    and `smbios/src/`), `legacy_memory_scan_source.h`/`.cpp` (moved from
    wherever Story 01.0 Task 02 lands it inside core `smbios`).
- **New:** `smbios/src/source_windows/` + `smbios/include/smbios/source_windows/`
  - `win_bios.h`/`.cpp`, `win_native_api_helper.h`, `win_handle_ptr.h`,
    `win_system_information.h`/`.cpp` (moved verbatim).
- **New:** `smbios/src/source_linux/` + `smbios/include/smbios/source_linux/`
  - `unix_bios.h`/`.cpp` (moved verbatim; still the same stub Story 02.0
    will later replace with real `LinuxSysfsSource`/`LinuxEfiSource`
    classes in this same directory).
- **New:** `smbios/src/source_macos/` + `smbios/include/smbios/source_macos/`
  - no file relocation (nothing macOS-specific exists today - macOS
    currently falls through to `unix_bios.h`'s stub via `smbios.cpp`'s
    `#else` branch); this story adds one small, real (not a logic
    reimplementation) `.cpp` defining `make_default_source_chain()` as
    `{LegacyMemoryScanSource}`, matching macOS's actual behavior today
    exactly, ready for Story 03.0 to add `MacOsUefiSource` alongside.
- **New:** five `CMakeLists.txt` files, one per directory above.
- **Modified:** `smbios/src/CMakeLists.txt` - adds the CMake-time
  `if(WIN32)/elseif(APPLE)/elseif(Linux)/else()` branch selecting which
  per-OS library (if any) `smbios` links, replacing the role
  `smbios.cpp`'s `#if` used to play.
- **Modified:** `smbios/include/smbios/smbios.h` / `smbios/src/smbios.cpp`
  - the `#if defined(_WIN32) ... win_bios.h ... #else ... unix_bios.h ... #endif`
    block at the top is deleted entirely; the constructor calls
    `smbios::make_default_source_chain()` with no `#if` around the call
    site (Story 01.0 Task 01 already wrote this call; only the now-fully-`#if`-free
    surrounding file changes here).
- **Not touched:** `smbios_util/`, `tests/unit/`, `tests/mock/`,
  `tests/perf/`, `tests/integration/` CMakeLists - all continue linking
  `smbios` only (Task 03 adds new unit-test targets for the new libraries
  without changing these).
