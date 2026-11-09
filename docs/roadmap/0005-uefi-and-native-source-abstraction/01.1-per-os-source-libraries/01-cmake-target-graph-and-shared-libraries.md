# 01 - CMake target graph and shared interface/legacy libraries

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Stand up the two platform-neutral CMake targets everything else in this
story and in Stories 02.0/03.0/04.0 depends on - `smbios_source_interface`
and `smbios_source_legacy` - and wire `smbios` (core) to link exactly one
per-OS library via a CMake-time branch instead of `smbios.cpp`'s current
C++ `#if defined(_WIN32)`. This is the resolution of contract **C3**
([plan.md](../plan.md), [ADR 0001](/docs/adr/0001-per-os-smbios-source-libraries.md)).

## Files

- **Create** `smbios/include/smbios/source_interface/` +
  `smbios/src/source_interface/CMakeLists.txt`:
  ```cmake
  add_library(smbios_source_interface INTERFACE)
  target_include_directories(smbios_source_interface INTERFACE
      ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
  ```
  Move `smbios/include/smbios/byte_reader.hpp` and
  `smbios/include/smbios/smbios_anchor.hpp` here verbatim (both are already
  fully header-only - no `.cpp` exists for either today, confirmed by
  `smbios/src/CMakeLists.txt`'s current `file(GLOB SOURCES *.cpp ...)`
  list). `smbios/include/smbios/ismbios_source.hpp` and
  `smbios/include/smbios/smbios_version_fixup.hpp` (both created by Story
  01.0, Tasks 01/02, inside core `smbios`'s own include tree) also move
  here - a relocation of files Story 01.0 already wrote, not a request to
  change Story 01.0's own task files.
  - **Note on target kind:** `smbios_source_interface` is a CMake
    `INTERFACE` library - it propagates include directories only and
    produces no `.a`/`.lib` artifact, because every file it owns is
    header-only. This is not a deviation from CLAUDE.md's "static
    libraries, not shared/dynamic" convention (that convention is about
    avoiding `.so`/`.dll` linkage, which is orthogonal); it is simply the
    correct CMake target kind for a set of files with nothing to compile.
    Record this reasoning in the target's own CMakeLists.txt comment so a
    future reader does not mistake it for an oversight.
- **Create** `smbios/include/smbios/source_legacy/` +
  `smbios/src/source_legacy/CMakeLists.txt`:
  ```cmake
  find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS iostreams REQUIRED)
  file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../include/smbios/source_legacy/*.h)
  add_library(smbios_source_legacy STATIC ${SOURCES})
  target_link_libraries(smbios_source_legacy PUBLIC smbios_source_interface ${Boost_LIBRARIES})
  target_compile_options(smbios_source_legacy PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
  ```
  Contents moved here by Task 02: `PhysicalMemory`/`NativePhysicalMemory`
  (today's `physical_memory.hpp`/`.cpp`, `posix_physical_memory.h`/`.cpp`,
  `win_physical_memory.h`/`.cpp` - the existing `#if`-split pair stays
  exactly as-is internally, just relocated) and `LegacyMemoryScanSource`
  (wherever Story 01.0 Task 02 places it inside core `smbios`).
- **Modify** `smbios/src/CMakeLists.txt`:
  ```cmake
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_interface)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_legacy)

  if(WIN32)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_windows)
      set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_windows)
  elseif(APPLE)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_macos)
      set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_macos)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source_linux)
      set(SMBIOS_PLATFORM_SOURCE_LIB smbios_source_linux)
  else()
      # BSD/Solaris/other *nix: no per-OS library exists; smbios core's
      # own fallback definition of make_default_source_chain() (added by
      # this task, guarded the same way) returns {LegacyMemoryScanSource}
      # only - identical to today's behavior on these targets.
      set(SMBIOS_PLATFORM_SOURCE_LIB "")
  endif()

  add_library(${TARGET} ${SOURCES})
  target_link_libraries(${TARGET} PUBLIC smbios_source_interface ${SMBIOS_PLATFORM_SOURCE_LIB} ${Boost_LIBRARIES})
  ```
  (exact variable/target names above are illustrative; reconcile with the
  file's existing `${TARGET}`-based conventions during implementation).
- **Modify** `smbios/include/smbios/smbios.hpp` / `smbios/src/smbios.cpp`:
  delete the `#if defined(_WIN32) ... #include "win_bios.hpp" ... #else
  ... #include "unix_bios.h" ... #endif` block at the top entirely (Story
  01.0 Task 01 already rewrote the constructor's body to call
  `make_default_source_chain()`; this task removes the now-dead `#if`
  that surrounded the old includes).

## Functions and Classes

- No new C++ classes - this task is CMake-target plumbing only.
- `smbios::make_default_source_chain()` - declaration only moves here
  (into `smbios_source_interface`'s header); its per-platform
  *definitions* are added by Task 02 (Windows/Linux skeletons) and
  Story 03.0 (macOS, once it exists) / this task's own fallback for
  BSD/Solaris.

## Constraints

- No `smbios_source_windows`/`smbios_source_macos`/`smbios_source_linux`
  target is ever linked together with another in the same build - the
  `if/elseif/elseif/else` chain above is mutually exclusive by
  construction.
- `smbios_source_legacy` must not depend on any per-OS library (DD2/DD3 in
  ADR 0001) - only on `smbios_source_interface`. Verify by grep: no
  `#include` from `source_legacy/` into `source_windows/`, `source_macos/`,
  or `source_linux/`.
- `smbios_util/CMakeLists.txt` is **not** modified - it links `smbios`
  only, unchanged, and must keep building without any awareness that
  `smbios` now has a deeper internal target graph.

## Tests

- Covered by Task 03's build-only verification (this task has no new
  runtime behavior to unit-test; it is pure target restructuring).

## Success criteria

- [ ] `smbios_source_interface` and `smbios_source_legacy` targets exist
      and build on every platform.
- [ ] `smbios/src/CMakeLists.txt`'s OS branch links exactly one per-OS
      library (or none, on BSD/Solaris) per configured platform.
- [ ] `smbios.cpp` contains zero `#if defined(_WIN32)` (or any platform
      macro) at its top - `grep -c "#if defined" smbios/src/smbios.cpp`
      reflects only whatever Story 01.0's own, unrelated logic still needs
      (expected: none, once this task lands).
- [ ] `smbios_util` and every existing test target still build unmodified.
