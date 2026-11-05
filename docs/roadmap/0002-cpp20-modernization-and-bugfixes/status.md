# Milestone 0002 - C++20 Modernization and Latent Bug Remediation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                          | Status         | Tests |
|-------|-------------------------------------------------|----------------|-------|
| 01.0  | Language standard raise and compiler hygiene   | ⬜ Not started | -     |
| 02.0  | Latent bug remediation                          | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Depends on milestone 0001 (vcpkg + CMake presets) being complete, so
  every fix in this milestone can be verified on Windows/Linux/macOS
  through the same build path.
- The four defects this milestone targets (`SMBiosVersion` comparison,
  `unix_bios.cpp`'s missing `return` in `get_table_size()`, the dishonestly
  "supported" `*nix` stubs, and the dead `// DEBUG` include) were identified
  by direct code review of `smbios/src/smbios.cpp` and
  `smbios/src/unix_bios.cpp`, not from a bug tracker - there is no external
  issue to cross-reference.
- **2026-11-04, confirmed by a real build attempt** (while verifying
  milestone 0001's Story 01.0 on macOS/AppleClang): defect #5 (the
  compiler-selection `if` chain never matching `AppleClang`) is not
  hypothetical - `cmake --build` on a clean vcpkg-based configure fails
  immediately on `abstract_smbios_entry.cpp`'s `std::make_unique` call
  with "no member named 'make_unique' in namespace 'std'", because
  `CMAKE_CXX_FLAGS` was empty (confirmed via `CMakeCache.txt`) and the
  compiler defaulted to a pre-C++14 standard. Forcing
  `-DCMAKE_CXX_STANDARD=14` externally gets past this specific file and
  immediately surfaces the second, unrelated defect recorded in milestone
  0005's `status.md` (macOS excluded from `posix_physical_memory.h`'s own
  `#if` guard) - so this codebase has never actually compiled end-to-end
  on macOS. Linux (`CMAKE_COMPILER_IS_GNUCXX`) and Windows (`WIN32`)
  branches both match their real `CMAKE_CXX_COMPILER_ID` correctly as
  written, so this specific defect is believed macOS-only - not yet
  confirmed on Linux/Windows (no CI exists yet; milestone 0001 Story 02.0
  adds it).
- **2026-11-05: defects #5 and #1 are both already fixed**, ahead of this
  milestone's own execution. Defect #5 (compiler-ID branch never matching
  `AppleClang`) was fixed by milestone 0001's Story 02.1
  (`fix-macos-build`), user-ratified as an explicit exception to that
  milestone's own scope - `STREQUAL "Clang"` → `MATCHES "Clang"`, plus the
  Clang branch's standard bumped to `gnu++14` (confirmed necessary even
  once matched). Defect #1 (`SMBiosVersion` comparison) was fixed and
  given regression coverage (`tests/unit/unit_tests.cpp`'s
  `SMBiosVersionGreater*`/`SMBiosVersionLess*` cases) as part of an
  ad hoc test-coverage improvement pass (also 2026-11-05, outside this
  roadmap's own sequencing - see milestone 0006's `status.md` for the
  fuller account). **Reconciliation for this milestone's Story 01.0 Task
  01 and Story 02.0 Task 01:** both specs are *consumed* already - verify
  the as-built fix matches each spec's stated approach exactly (it does,
  by direct comparison) rather than re-implementing; what remains of
  Story 01.0 is the broader C++20 raise (`CXX_STANDARD 20` everywhere,
  `-Werror`, sanitizer CI) and of Story 02.0 the other three defects
  (#2 missing `return`, #3 dishonest stubs, #4 dead `// DEBUG` include),
  none of which are touched by either fix above.
