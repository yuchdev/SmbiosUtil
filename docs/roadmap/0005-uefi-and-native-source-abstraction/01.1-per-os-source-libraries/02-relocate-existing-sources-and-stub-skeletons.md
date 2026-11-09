# 02 - Relocate existing per-OS files; macOS and Linux skeletons

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Populate `smbios_source_windows` and `smbios_source_linux` with the real
files that already exist today (moved verbatim, zero logic change), and
give `smbios_source_macos` a minimal, honest skeleton - since, unlike
Windows and Linux, there is no macOS-specific source file anywhere in this
codebase today to relocate (macOS currently falls through to the same
stub `unix_bios.h`/`.cpp` Linux uses, via `smbios.cpp`'s `#else` branch).

## Files

- **Create** `smbios/src/source_windows/CMakeLists.txt`:
  ```cmake
  file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_windows/*.h)
  add_library(smbios_source_windows STATIC ${SOURCES})
  target_link_libraries(smbios_source_windows PUBLIC smbios_source_interface smbios_source_legacy)
  target_compile_options(smbios_source_windows PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
  ```
  **Move** `smbios/include/smbios/win_bios.hpp`, `smbios/src/win_bios.cpp`,
  `smbios/include/smbios/win_native_api_helper.hpp`,
  `smbios/include/smbios/win_handle_ptr.hpp`,
  `smbios/include/smbios/win_system_information.hpp`,
  `smbios/src/win_system_information.cpp` into
  `smbios/include/smbios/source_windows/` / `smbios/src/source_windows/`
  verbatim - **no edit to any Windows API call site**, matching this
  story's "zero behavior change" constraint. `win_bios.cpp` keeps
  constructing/using `SMBiosImpl` exactly as it does today; Story 04.0's
  Task 01 is still the one that adds `: public ISmbiosSource` to it and
  defines `make_default_source_chain()` here as
  `{WindowsNativeSource-or-adapted-SMBiosImpl, LegacyMemoryScanSource}` -
  until Story 04.0 lands, this task adds a **temporary**, clearly-marked
  `make_default_source_chain()` definition in this library that
  constructs the existing `SMBiosImpl` directly through a thin inline
  adapter (not a redesign - the minimum needed to keep `smbios` linkable
  and behaviorally unchanged between this story and Story 04.0).
- **Create** `smbios/src/source_linux/CMakeLists.txt` (same shape as
  Windows's, substituting the target name and linked libraries).
  **Move** `smbios/include/smbios/unix_bios.h`,
  `smbios/src/unix_bios.cpp` into
  `smbios/include/smbios/source_linux/` / `smbios/src/source_linux/`
  verbatim - still today's stub (`sysfs_table_exists()` hardcoded
  `false`, `efi_table_exists()` detection-only, `reading_from_efi()`/
  `reading_from_sysfs()` empty) - Story 02.0 is still what replaces this
  with real `LinuxSysfsSource`/`LinuxEfiSource` classes in this same
  directory. Add a temporary `make_default_source_chain()` definition
  here, the same shape as Windows's temporary one, returning
  `{LegacyMemoryScanSource}` only (today's actual Linux behavior, since
  the stub never succeeds).
- **Create** `smbios/src/source_macos/CMakeLists.txt` (same shape).
  **No file to move.** Add one new, real (if minimal) `.cpp`:
  ```cpp
  // smbios/src/source_macos/smbios_source_chain.cpp
  // Temporary: macOS has no native ISmbiosSource implementation yet.
  // Story 03.0 adds MacOsUefiSource alongside this file; until then,
  // this definition reproduces today's actual macOS behavior exactly
  // (legacy scan only - macOS previously fell through to unix_bios.h's
  // stub via smbios.cpp's #else branch, which never succeeded either).
  std::vector<std::unique_ptr<ISmbiosSource>> smbios::make_default_source_chain() {
      std::vector<std::unique_ptr<ISmbiosSource>> chain;
      chain.push_back(std::make_unique<LegacyMemoryScanSource>());
      return chain;
  }
  ```
- **Modify** `smbios/src/CMakeLists.txt`'s BSD/Solaris `else()` branch
  (Task 01): add the equivalent trivial fallback definition, compiled
  directly into `smbios` core only in that one branch (since no per-OS
  library exists for it) - identical shape to the macOS stub above.

## Functions and Classes

- No new `ISmbiosSource` subclass - only the three temporary
  `make_default_source_chain()` definitions described above, each
  reproducing exactly today's real, already-shipping behavior on its
  platform (Windows: native source succeeds; Linux/macOS/BSD/Solaris:
  legacy scan only).

## Constraints

- **Windows must not regress.** The temporary adapter in
  `smbios_source_windows` is the one place in this task where "skeleton
  only" and "zero behavior change" could conflict if done carelessly -
  resolve by keeping the adapter a thin, inline wrapper calling
  `SMBiosImpl`'s existing methods, not a reimplementation. Record in
  `status.md` whether this temporary adapter survives unchanged into
  Story 04.0 or is replaced outright by that story's own Task 01 (either
  is acceptable; the decision just needs to be recorded, following this
  milestone's established practice for this kind of open implementation
  choice).
- `unix_bios.h`'s existing class-level doc comment is **not** rewritten
  in this task (Story 02.0 already owns that, per its own Key
  constraints) - only its file location changes.
- No change to `win_native_api_helper.hpp`'s Windows API calls (Story
  04.0's own constraint, inherited here since this task moves the file
  that contains them).

## Tests

- See Task 03.

## Success criteria

- [ ] `smbios_source_windows` builds on Windows and produces
      byte-identical `SMBios` output to pre-this-story.
- [ ] `smbios_source_linux` builds on Linux; `source_name()` still
      reports the legacy-scan fallback (no behavior change - Story 02.0
      has not landed yet).
- [ ] `smbios_source_macos` builds on macOS; `source_name()` reports the
      legacy-scan fallback, matching today's actual (if previously
      accidental) behavior.
- [ ] No Windows API call site, and no line of `unix_bios.cpp`'s actual
      stub logic, changed by this task - `git diff` on both moved files
      shows a pure rename/move, not a content edit (aside from any
      `#include` path updates the relocation itself requires).
