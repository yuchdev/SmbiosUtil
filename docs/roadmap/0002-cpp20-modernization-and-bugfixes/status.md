# Milestone 0002 - C++20 Modernization and Latent Bug Remediation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                          | Status         | Tests |
|-------|-------------------------------------------------|----------------|-------|
| 01.0  | Language standard raise and compiler hygiene   | 🔶 In progress (2/3 tasks) | smbios_unit_test, smbios_mock_test pass |
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
- **2026-11-05: both fixes confirmed by a real, green, three-OS CI run**
  (`https://github.com/yuchdev/SmbiosUtil/actions/runs/41996903863`), not
  just local AppleClang testing - see milestone 0001's `status.md` for
  the full push-by-push account. That same CI run also surfaced several
  *additional* portability defects neither this milestone's plan nor the
  original macOS investigation had found (missing `<cstdint>`/`<limits>`/
  `<cstdlib>`/`<clocale>`/`<unistd.h>`/`<string>`/`<Windows.h>` includes,
  a `class`/`struct` forward-declaration mismatch, a hardcoded CMake
  generator version) - all fixed directly in milestone 0001 rather than
  deferred here, since none of them are specific to the C++20/compiler-
  flag scope this milestone's own Story 01.0 owns.
- **2026-11-05, correction found at `implement-milestone` cold-start R3**
  (direct read of the root `CMakeLists.txt`, not a status.md re-read): the
  entry above overstated Story 01.0 Task 01's state. Only the *narrow*
  AppleClang-matching fix landed in milestone 0001 Story 02.1
  (`STREQUAL "Clang"` → `MATCHES "Clang"`, standard bumped to `-std=gnu++14`
  on that branch only) - `CMAKE_CXX_STANDARD 20` /
  `CMAKE_CXX_STANDARD_REQUIRED ON` / `CMAKE_CXX_EXTENSIONS OFF` were never
  set, and the GCC/MSVC branches still use `-std=gnu++1y` /
  compiler-default respectively. Task 01's own spec
  (`01.0-.../01-raise-to-cpp20.md`) requires the full uniform C++20 raise
  via `CXX_STANDARD` properties, not string-matched flags - that has not
  happened. **Reconciliation:** Story 01.0 Task 01 is *not* consumed;
  reverting to `not_started`. Only Story 02.0 Task 01 (`SMBiosVersion`
  comparison) remains genuinely consumed - confirmed independently by
  reading `smbios/src/smbios.cpp`'s current `operator>`/`operator<` and
  `tests/unit/unit_tests.cpp`'s `SMBiosVersionGreater*`/`SMBiosVersionLess*`
  cases, which match the fix already quoted in milestone 0001's
  closing-summary commit history.
- **2026-11-05: Story 01.0 Task 02 (warnings-as-errors) landed, and it
  subsumed defect #2** (the missing `return` in
  `unix_bios.cpp`'s `get_table_size()`) as a direct, intended consequence
  of turning on `-Werror` - exactly as this milestone's own
  "Why standard-raise before bugfixes" rationale predicted. Confirmed by a
  manual regression proof (reintroducing the bug reproduced a real
  `-Wreturn-type` build failure, then reverted) and independently
  re-verified by direct read of the fixed line. **Reconciliation for Story
  02.0 Task 02:** the `return`-statement half of that task is *consumed*;
  its remaining scope is only the dishonest-stub documentation correction
  (the stale "has been reported to work on FreeBSD, NetBSD, OpenBSD, BeOS
  and Solaris" class doc comment, and `sysfs_table_exists()`'s hardcoded
  `#if 1 return false`) - deliberately left untouched by this task so the
  two tasks' scopes don't collide. Also fixed in the same pass: 8
  `-Wpessimizing-move` sites (explicit `std::move` on a prvalue return
  defeating NRVO/copy elision) and one `-Wunused-parameter` (a
  `PortConnectionEntry` constructor parameter marked `[[maybe_unused]]`,
  not removed, to keep its signature matching sibling entry classes) - all
  pre-existing warnings with no behavior change. A new `sanitizers` CI job
  (Linux+macOS, ASan+UBSan, mirroring milestone 0001's unprivileged-subset
  pattern) was added to `.github/workflows/ci.yml`; a real
  `detect_container_overflow` libc++/ASan false positive was found during
  local verification (`AbstractSMBiosEntry::parse_dmi_strings()`'s
  initializer-list-constructed `vector<string>` reallocating) and
  suppressed via `ASAN_OPTIONS`, confirmed not a real defect by toggling
  the option and observing the test pass. GCC/MSVC branches and the
  sanitizer job itself are not yet confirmed green - only AppleClang was
  locally verifiable; real confirmation needs a CI push.
