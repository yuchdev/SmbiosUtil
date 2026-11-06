# Milestone 0005 - Native Source Abstraction and UEFI Support - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                               | Status         | Tests |
|-------|---------------------------------------|----------------|-------|
| 01.0  | `ISmbiosSource` strategy interface   | ✅ Complete | smbios_unit_test pass (150 cases); real CI green incl. Windows |
| 01.1  | Per-OS source libraries                | ✅ Complete | smbios_unit_test pass (152 cases); local macOS build/test green, Windows/Linux CI confirmation pending next push |
| 02.0  | Linux sysfs & EFI sources             | ⬜ Not started | -     |
| 03.0  | macOS UEFI source                     | ⬜ Not started | -     |
| 04.0  | Windows source parity                 | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This is the milestone implementing the headline ask: "add UEFI support
  for Mac and Linux." It is scoped against the specific gaps milestone
  0002 documented rather than fixed - `unix_bios.cpp`'s
  `sysfs_table_exists()` hardcoded `false`, `reading_from_efi()`/
  `reading_from_sysfs()` empty bodies - and against CLAUDE.md's own
  "Mac has no native SMBIOS source (README asks for a UEFI implementation)"
  statement.
- **Resolved (previously flagged as an open risk needing a hardware
  spike).** `c-versions/dmidecode_macos` and `c-versions/dmidecode_linux`
  are **not** empty placeholders - an earlier pass of this roadmap
  inspected them with a non-following `find` and missed that both are
  symlinks to real, working reference checkouts (the Acidanthera
  dmidecode fork for macOS; upstream dmidecode for Linux). Reading them
  directly resolved the macOS mechanism with no hardware spike needed:
  `IORegistryEntryFromPath(kIOMasterPortDefault,
  "IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS")`, property
  `"SMBIOS-EPS"` for the entry point and `"SMBIOS"` for the raw table
  (`c-versions/dmidecode_macos/util.c`'s `mem_chunk_ioreg()`). This
  superseded the original working assumption (an `IODeviceTree:/efi/platform`
  node), which was wrong. Story 03.0's plan and Task 01 are rewritten
  around the confirmed mechanism; see `plan.md`'s "Reference material"
  section for the one deliberate deviation from the reference (Apple
  Silicon architecture gating).
- Per the same re-read, Linux's sysfs path/EFI-systab format (`plan.md`
  R4) are also now confirmed, not merely "the known technique," against
  `c-versions/dmidecode_linux`.
- **New finding, not in the original plan:** `dmidecode_linux`'s
  `dmi_fixup_version()` corrects known-malformed BIOS-reported SMBIOS
  versions (`2.31`→`2.3`, `2.33`→`2.6`, `2.51`→`2.6`) before use. This
  codebase has no equivalent today. Added as **R9** and folded into Story
  01.0, Task 02 (entry-point version extraction already lives there,
  post-refactor) rather than opening a new task.
- **New finding, confirmed 2026-11-04 by a real build attempt** (while
  verifying milestone 0001's Story 01.0): CLAUDE.md's "Mac has no native
  SMBIOS source" is an understatement - `smbios/src/physical_memory.cpp`'s
  `#if defined(_WIN32)... #elif defined(__linux__) || __FreeBSD__ ||
  __NetBSD__ || __OpenBSD__ || __sun ... #endif` (and
  `posix_physical_memory.h`'s identical guard) **excludes `__APPLE__`
  entirely**, so `NativePhysicalMemory` has no concrete definition at all
  on macOS, yet `PhysicalMemory`'s constructor/destructor still
  instantiate `std::unique_ptr<NativePhysicalMemory>`'s deleter against
  it. Confirmed as a real compile failure (`error: allocation of
  incomplete type 'smbios::NativePhysicalMemory'`) once milestone 0002's
  compiler-standard defect (that milestone's defect #5) was worked around
  locally to get far enough to reach it. **Today, `smbios` does not
  compile on macOS at all** - not "falls back to a scan," a hard build
  error. This sharpens, rather than changes, this milestone's own
  rationale: Story 03.0's `MacOsUefiSource` is what finally gives macOS a
  path that compiles and works, and Story 01.0's `LegacyMemoryScanSource`
  extraction (Task 02) should either extend this `#if` guard to include
  `__APPLE__` as a real (if likely SIP-restricted, low-value) fallback, or
  explicitly document that macOS's chain has no legacy-scan fallback at
  all and relies entirely on `MacOsUefiSource` - a decision for Story
  01.0 Task 02's implementer to make and record, not pre-judged here.
- **2026-11-05: the `__APPLE__`-exclusion defect itself is now fixed**
  (milestone 0001's Story 02.1, confirmed by a real green 3-OS CI run -
  see that milestone's `status.md`), ahead of this milestone's own
  execution. `smbios` now compiles and links on macOS; it still has no
  *real* native source there - the legacy scan's `/dev/mem` path fails
  cleanly at runtime on modern macOS (confirmed by an actual unprivileged
  run: no such device node exists at all). This milestone's
  `MacOsUefiSource` (Story 03.0) is still what macOS actually needs to
  produce real data; the fix above only removed the "doesn't even
  compile" floor.
- **2026-11-06: new Story 01.1 added, user-requested, after this
  milestone's four stories were already numbered.** The user asked that
  `smbios` become "completely cross-platform" not just via the runtime
  `ISmbiosSource` chain Stories 01.0-04.0 already planned, but by
  packaging each platform's implementation as its **own library** - their
  own words allowed for more than 3 ("could be 3 for Windows, macOS,
  Linux, but if the architecture favors more ... go for it"). Presented as
  a choice (extend this milestone vs. force it into milestone 0003 vs. a
  new milestone); the user chose to extend this milestone. Inserted as
  **01.1** per this roadmap's own `{TT.t}` sub-story convention (the same
  pattern milestone 0001 used for `02.1-fix-macos-build`), landing between
  Story 01.0 (needs `ISmbiosSource` to exist) and Stories 02.0/03.0/04.0
  (which now build their real source logic directly inside their own
  library from day one, instead of being refactored into that shape
  later).
  - **Library-count decision, recorded in full in [ADR 0001](/docs/adr/0001-per-os-smbios-source-libraries.md):**
    five targets, not a literal reading of the user's "3, maybe 5 for
    Linux" example - `smbios_source_interface` (header-only `INTERFACE`
    library: `ISmbiosSource`, the chain-factory *declaration*,
    `fixup_smbios_version()`, `read_uN`, `detect_smbios_anchor()`),
    `smbios_source_legacy` (the already-OS-agnostic legacy
    physical-memory scan, shared by all three platforms' chains and by
    Linux's EFI source), and exactly one real per-OS library each for
    Windows/macOS/Linux. The reasoning, in short: a library boundary only
    pays for itself when some build wants one piece without another. That
    is true for the OS boundary itself (this project's CMake presets
    already build for exactly one OS at a time) and true for the legacy
    scan (genuinely reused four ways, with zero platform-specific code of
    its own beyond the already-existing `PhysicalMemory` PImpl split) -
    but **not** true for splitting Linux's sysfs and EFI sources from
    each other, since nothing in this project ever wants to link one
    without the other; they stay in one `smbios_source_linux` library.
    Keeping `ISmbiosSource`/decode primitives inside core `smbios` instead
    of a separate interface library was also considered and rejected: it
    would create a dependency cycle the moment `SMBios`'s constructor
    needs a symbol (`make_default_source_chain()`) only a per-OS library
    defines, while every per-OS library simultaneously needs decode
    primitives (`read_uN`, `detect_smbios_anchor()`) that live in core
    `smbios` today.
  - **R8's resolution sharpens as a result.** Story 01.0, Task 01's
    original plan kept "a minimal remaining `#if`" inside
    `smbios_source_chain.cpp`. Story 01.1 supersedes that one detail:
    `make_default_source_chain()` is defined exactly once per per-OS
    library (a link-seam CMake resolves by choosing which library to
    link), so `smbios.cpp` and `smbios_source_chain.cpp` end up with
    **zero** `#if defined(_WIN32)` at all, not merely "minimal." Nothing
    else about Story 01.0's own plan changes - this is a packaging
    refinement of where Task 01's output physically compiles, not a
    redesign of the interface or chain-walking logic itself.
  - Stories 02.0/03.0/04.0's "Depends on" lines and file-path references
    are updated with a one-line amendment each (now also depending on
    Story 01.1's library skeleton, and creating their files inside
    `smbios/{include,src}/smbios/source_<os>/` instead of the flat
    `smbios/{include,src}/smbios/` layout originally planned) - their
    actual source-reading logic is unchanged.
- **2026-11-07, decision ratified before Story 01.0 Task 01 execution:**
  reading the real current `smbios.cpp` in full surfaced a contract-level
  fork Task 01's own spec example doesn't resolve - `SMBios::
  render_to_description()` today prints the raw entry-point struct's
  fields (checksum, entry-point revision, BCD version, intermediate
  checksum) directly from `smbios_entry32_`/`smbios_entry64_`, members
  that the new one-source-per-platform chain model encapsulates away
  inside whichever concrete `ISmbiosSource` succeeds. Task 01's own
  "byte-identical results" success criterion cannot literally apply to
  this method without re-exposing that struct back through the
  interface, partly undercutting the encapsulation the refactor is for.
  Presented to the user as a fork (simplify vs. preserve byte-for-byte
  vs. defer to the implementing agent); **ratified: simplify**.
  `render_to_description()` becomes a diagnostic report of SMBIOS
  version, table size, structure count, and which source succeeded -
  the detailed entry-point struct dump is dropped. Nothing in
  `smbios_util`'s CLI or any existing test depends on that output's
  exact text (confirmed before asking). The "byte-identical" success
  criterion is understood to apply to `get_table_base()`/
  `get_table_size()`/`get_smbios_version()`/the parsed `headers_list_`
  contents - the actual data a caller consumes - not to this one
  diagnostic method's prose.
- **2026-11-07: Story 01.0 Task 01 complete.** `ISmbiosSource` (C1) and
  `make_default_source_chain()` (C2) implemented in
  `smbios/include/smbios/ismbios_source.h` /
  `smbios/include/smbios/smbios_source_chain.h` /
  `smbios/src/smbios_source_chain.cpp`. `SMBios::SMBios()`'s constructor
  is now entirely `#if`-free, walking the chain generically - the one
  remaining `#if defined(_WIN32)` in the whole native-read path lives
  inside `make_default_source_chain()` itself, selecting which
  `ISmbiosSource` exists on this platform (R8's resolution, ahead of
  Story 01.1's own further resolution into a CMake-level link-seam).
  Per this task's own stated constraint, every platform's chain today
  has exactly one element (`WindowsNativeSource`/
  `UnixNativeOrLegacyScanSource`), internally reproducing today's exact
  native-then-legacy-scan behavior - splitting that into separate
  sources is Task 02's job, not done here.

  All three previously-landed bug fixes (milestone 0003's two exception-
  safety `try`/`catch` blocks around `PhysicalMemory` access, and the
  null-pointer-arithmetic UB guard in `read_smbios_table()`/
  `count_smbios_structures()`) are preserved byte-for-byte - independently
  re-verified here by direct file read, not just trusted from the
  implementer's own report. `SMBios` gained one new member
  (`table_buffer_`, populated directly by whichever source's
  `try_read()` succeeds) and lost five (`native_impl_`,
  `entry_point_buffer_`, `smbios_entry32_`/`smbios_entry64_`,
  `checksum_validated_`, `devmem_base_`/`devmem_length_`) - all genuinely
  removed, not hidden; the old `scan_physical_memory()`/
  `extract_dmi_version()`/`checksum32()`/`checksum64()`/`smbios_crc()`
  private methods moved into `smbios_source_chain.cpp` as free
  functions, confirmed line-by-line identical to the pre-refactor logic.
  `render_to_description()` now prints SMBIOS version, table size,
  structure count, and the succeeding source's name (or "none") per the
  ratified simplification - confirmed live via a real `smbios_util` run
  on this unprivileged Mac (`Source: none`, matching the expected
  no-native-source-available state).

  `WindowsNativeSource` - written unverified against `win_bios.h`'s real
  interface, no Windows toolchain in this session - **confirmed by real
  CI** (`https://github.com/yuchdev/SmbiosUtil/actions/runs/42290814193`:
  `build (windows-latest)` succeeded, alongside all 4 other jobs). The
  shared `legacy_physical_memory_scan()` fallback it reuses is itself
  already platform-neutral (not new Windows-specific code), matching
  R6's "adapt, don't rewrite" mandate.

  Verified independently: direct file read of both exception-safety
  fixes, the null-pointer-arithmetic guard, the checksum/scan logic
  (confirmed identical to the pre-refactor version), a real local macOS
  rebuild, a full `ctest` run (`smbios_unit_test` 142/142 cases,
  `smbios_integration_test`/`smbios_perf_test` behaving exactly as the
  pre-existing baseline - no new crash or uncaught exception), and a
  live `smbios_util` CLI run confirming the new `render_to_description()`
  output format.
- **2026-11-07, reconciliation found before starting Task 02:** Task
  02's own spec file (`02-fallback-policy-and-legacy-scan-isolation.md`)
  predates the `render_to_description()` decision above and still says
  the method's "two-branch format (32-bit vs. 64-bit entry point) must
  still be reachable and byte-identical when the legacy source is what
  succeeded," naming this as one of its own success criteria. That
  requirement is **superseded** by the already-ratified-and-shipped
  simplification - the user settled this exact question for Task 01,
  not provisionally, and Task 01's simplified `render_to_description()`
  is already merged and CI-confirmed. Task 02 proceeds on its real
  remaining scope (extracting `scan_physical_memory`/
  `extract_dmi_version`/`checksum32`/`checksum64`/`smbios_crc` into
  `LegacyMemoryScanSource`, adding `fixup_smbios_version()` per R9) -
  the detailed entry-point dump does **not** come back, and
  `render_to_description()` is not touched by this task at all.
- **2026-11-07: Story 01.0 Task 02 complete.** `LegacyMemoryScanSource`
  (`smbios/include/smbios/legacy_memory_scan_source.h`/`.cpp`) is now a
  real, standalone `ISmbiosSource` - the scan/checksum logic that Task
  01 had already moved once (from `SMBios` into free functions in
  `smbios_source_chain.cpp`) now lives a second and final time inside
  this class. Both `WindowsNativeSource::try_read()` and
  `UnixNativeOrLegacyScanSource::try_read()` confirmed (by direct file
  read) to delegate to `LegacyMemoryScanSource{}.try_read(...)` instead
  of calling a free function - the old free functions are gone from
  `smbios_source_chain.cpp` entirely. `fixup_smbios_version()`
  (`smbios/include/smbios/smbios_version_fixup.h`, R9) ported and
  independently re-verified against `dmi_fixup_version()`
  (`c-versions/dmidecode_linux/dmidecode.c:5967`): packed `0x021F`/
  `0x0221` → corrected to major 2/minor 3, packed `0x0233` → major
  2/minor 6, everything else passes through unchanged - called from
  `LegacyMemoryScanSource::try_read()` before the real table mapping
  stage, genuinely new correctness this codebase never had.
  `render_to_description()` confirmed untouched by direct `git diff`
  (empty) against both `smbios.cpp` and `smbios.h` - the reconciliation
  above held. Verified independently: direct file read of the fixup
  table, the delegation call sites, and the diff confirming
  `render_to_description()` wasn't touched; a real local macOS rebuild;
  a full `ctest` run (`smbios_unit_test` 150/150 cases,
  `smbios_integration_test`/`smbios_perf_test` unchanged from baseline);
  and a live `smbios_util` CLI run confirming the same `Source: none`
  diagnostic output as before this task.

### Story 01.0 - ISmbiosSource Strategy Interface (✅ 2026-11-07)

**Delivered:** `ISmbiosSource` (C1) and `make_default_source_chain()`
(C2); `SMBios::SMBios()`'s constructor is now entirely `#if`-free,
walking the chain generically - the sole remaining `#if defined(_WIN32)`
lives inside `make_default_source_chain()` itself, selecting which
source type exists on this platform (R8). `LegacyMemoryScanSource` is a
real, standalone `ISmbiosSource` both platform wrappers delegate to.
`fixup_smbios_version()` (R9) is new, additive correctness this codebase
never had before. All three of milestone 0003's hard-won fixes
(two exception-safety `try`/`catch` blocks, one null-pointer-arithmetic
UB guard) preserved byte-for-byte, independently re-verified by direct
file read at every task, not merely trusted from agent reports.

**Tests / gate:** `smbios_unit_test` 150/150 cases (8 new this story:
`smbios_source_chain_test.cpp` ×3, `smbios_version_fixup_test.cpp` ×5,
plus `legacy_memory_scan_source_test.cpp` ×3 - 11 new cases total across
both tasks). Confirmed by real, green 5-job CI on both pushes
(`https://github.com/yuchdev/SmbiosUtil/actions/runs/42290814193` for
Task 01, including `build (windows-latest)` succeeding on the
previously-unverified `WindowsNativeSource`). `smbios_integration_test`/
`smbios_perf_test` behave identically to the established pre-story
baseline throughout - no new crash, no new uncaught exception.

**Reconciliation note:** both Task 02's and Task 03's own spec files
name a "byte-identical `render_to_description()`" success criterion
that predates a decision ratified before Task 01 even started -
`render_to_description()` was simplified (version/size/count/source-
name) rather than preserved byte-for-byte, since the old detailed
entry-point-struct dump is exactly the internal state the new chain
model encapsulates away, and nothing depended on its exact prior text.
Disposition: both specs' entry-point-dump criterion is *superseded*,
not applicable going forward; the simplified format is itself stable
across every task in this story (confirmed unchanged by `git diff`
after Task 02), which is the real substance of "byte-identical... before
Stories 02.0-04.0 add real sources on top of it" that Task 03's gate
actually cares about.
- **2026-11-07: Story 01.1 Tasks 01 and 02 complete, done together in
  one pass** - Task 01's own `smbios/src/CMakeLists.txt` snippet
  references `source_windows`/`source_macos`/`source_linux`
  subdirectories that don't exist until Task 02 creates them; doing
  Task 01 in isolation would have left a non-configuring, broken build
  (`add_subdirectory()` on a nonexistent path), the same "clean
  coexistence infeasible" situation milestone 0004's Story 01.0 Task 01
  hit for an analogous reason. Final target graph:
  `smbios_source_interface` (`INTERFACE`, header-only: `ISmbiosSource`,
  the chain-factory declaration, `byte_reader.h`, `smbios_anchor.h`,
  `smbios_version_fixup.h`, and a newly-relocated
  `smbios_entry_point.h`) ← `smbios_source_legacy` (`STATIC`:
  `PhysicalMemory`/`NativePhysicalMemory`, `LegacyMemoryScanSource`) ←
  exactly one of `smbios_source_windows`/`smbios_source_macos`/
  `smbios_source_linux` (`STATIC`), selected by a CMake-time
  `if(WIN32)/elseif(APPLE)/elseif(Linux)/else()` branch replacing the
  role `smbios.cpp`'s old `#if defined(_WIN32)` played (R8) - confirmed
  by direct grep, zero real preprocessor platform directives remain in
  `smbios.cpp` (one comment-text match only).

  **Two real spec-vs-ADR corrections, found and fixed during
  implementation, not pre-judged by either task's own file list:**
  `SMBIOSEntryPoint32`/`SMBIOSEntryPoint64` (used only by
  `LegacyMemoryScanSource`, assigned to `smbios_source_legacy`) were
  extracted out of core `smbios.h` into
  `source_interface/smbios_entry_point.h` - leaving them in `smbios.h`
  would have made `smbios_source_legacy` depend on core `smbios`,
  backwards from ADR 0001's intended acyclic direction.
  `win_native_api_helper.h`/`win_handle_ptr.h` (used only by
  `win_physical_memory.cpp`, itself assigned to `smbios_source_legacy`,
  not `win_bios.cpp`) were moved into `source_legacy`, not
  `source_windows` as Task 02's literal file list said - moving them to
  `source_windows` would have made the "shared, no per-OS dependency"
  library depend on a per-OS one, violating the same acyclic rule.
  Both corrections independently re-verified here by direct `find`/
  `grep`, not merely trusted from the report.

  `smbios_source_chain.cpp` (Story 01.0's temporary home for
  `WindowsNativeSource`/`UnixNativeOrLegacyScanSource`) is deleted; its
  content is redistributed into `source_windows/windows_native_source.cpp`
  and `source_linux/linux_source_chain.cpp` respectively (each keeping
  its own small `copy_native_result()` helper duplicated rather than
  shared - documented as temporary pending Story 02.0/04.0, when each
  platform's real source classes land and this duplication likely
  resolves naturally). A new `fallback_source_chain.cpp` supplies
  `make_default_source_chain()` for the BSD/Solaris `else()` branch
  only - excluded from the default `file(GLOB)` and re-added
  explicitly just there, avoiding a duplicate-definition link error on
  every platform that does have a per-OS library (a real risk the
  implementer caught and solved correctly, independently confirmed by
  reading the final `smbios/src/CMakeLists.txt`).

  `git diff`-confirmed pure moves (no logic change, only `#include`
  path updates) for `win_bios.cpp`/`win_bios.h` and
  `unix_bios.cpp`/`unix_bios.h` - re-verified directly here via `diff`
  against the pre-move committed content, not just trusted from the
  report. `tests/unit/unit_tests.cpp`'s `SMBiosImpl`-based regression
  test narrowed from "any unix-like platform" to `__linux__` only,
  since `SMBiosImpl`/`unix_bios.h` now live exclusively in the
  Linux-only `smbios_source_linux` library - a real, well-documented
  scope narrowing (the test still exists and will run on real Linux
  CI), not a silent coverage loss; confirmed by direct read of the new
  guard and its doc comment.

  Verified independently: the directory/target-graph structure, the
  `#if` removal, the acyclic-dependency grep check, both pure-move
  diffs, a real local macOS rebuild through the entire new target
  graph (`smbios_source_interface` → `smbios_source_legacy` →
  `smbios_source_macos` → `smbios` core → `smbios_util`/tests), and a
  full `ctest` run (`smbios_unit_test` 149/149 cases - one fewer than
  before, exactly accounted for by the documented Linux-only test
  narrowing, not a loss). Windows (`smbios_source_windows`) and Linux
  (`smbios_source_linux`) library compilation remain unverified by a
  real compile on this macOS sandbox, same as every per-OS-specific
  claim throughout this milestone - **confirmed by real CI**
  (`https://github.com/yuchdev/SmbiosUtil/actions/runs/42295605341`:
  all 5 jobs green, `build (windows-latest)` and `build (ubuntu-latest)`
  included).
- **2026-11-07: Story 01.1 Task 03 complete, closing this story.** Two new
  `tests/unit/` cases prove the link-seam itself, not just behavior already
  covered elsewhere: `smbios_source_legacy_link_test.cpp` (2 cases:
  `is_available()`/`source_name()` against `LegacyMemoryScanSource`, the
  same contract `legacy_memory_scan_source_test.cpp` already asserts,
  re-stated here as the DD2/DD3 link-graph proof from
  [ADR 0001](/docs/adr/0001-per-os-smbios-source-libraries.md) rather than
  new behavior coverage) and `smbios_source_chain_link_seam_test.cpp` (1
  case: `BOOST_CHECK(!make_default_source_chain().empty())`, confirming
  `smbios` core's link graph resolves the symbol with no unresolved
  reference on whatever per-OS library this platform's CMake branch
  selected).

  `LegacyMemoryScanSource::source_name()` confirmed by direct file read
  (`smbios/src/source_legacy/legacy_memory_scan_source.cpp`) to return the
  exact literal `"legacy-scan"`, and `is_available()` to unconditionally
  `return true` - both already asserted by the pre-existing
  `legacy_memory_scan_source_test.cpp`, re-verified rather than assumed
  before writing the new test's assertions.

  **CMake wiring: no change needed.** The spec's own suggested
  `target_link_libraries(smbios_unit_test PRIVATE smbios_source_legacy)`
  addition was tried first by omission, per this task's own instruction to
  check before adding it: a real local build of `smbios_unit_test` with
  only the two new `.cpp` files added (picked up automatically by
  `tests/unit/CMakeLists.txt`'s existing `file(GLOB SOURCES *.cpp)`) linked
  and ran cleanly with no unresolved symbol. `smbios_unit_test`'s existing
  `target_link_libraries(... smbios)` already pulls in
  `LegacyMemoryScanSource`'s symbols transitively, because
  `smbios/src/CMakeLists.txt` links `smbios_source_legacy` into `smbios`
  core as `PUBLIC`, not `PRIVATE` - so no extra `target_link_libraries`
  entry was added. The one real CMake-adjacent change this task makes is
  therefore zero lines in any `CMakeLists.txt`; both new test files are
  picked up by the existing glob alone.

  `tests/mock/mock_tests.cpp`'s `NoMockableSeamYetTestCase` comment is
  updated (not removed - no concrete fake `ISmbiosSource` substitution test
  exists yet, which is Stories 02.0/03.0/04.0's job) to state that the
  `ISmbiosSource` seam and its library boundary (this story) both now
  exist, and the suite's remaining blocker is a concrete fake source, not a
  missing architectural seam.
  [docs/test/test-subsystem.md](/docs/test/test-subsystem.md)'s own
  pointer to this story's two link-seam tests was already added when Story
  01.1's commit landed (`30e16c7`, ahead of this task) and needed no
  further edit - re-verified here by direct read rather than assumed
  already correct.

  `render_to_description()` is untouched by this task, per its own
  constraint - it is not part of this task's scope and the already-ratified
  simplified format (Story 01.0, Task 01) remains the current, stable
  contract.

  Verified independently: direct file read of
  `LegacyMemoryScanSource::is_available()`/`source_name()`'s real bodies
  before asserting against them in the new test; a real local macOS
  rebuild (`cmake --preset macos && cmake --build --preset macos`)
  confirming the build is warning-clean and links without the spec's
  suggested extra `target_link_libraries` entry; a full `ctest` run
  (`smbios_unit_test` 152/152 cases - 149 plus the 3 new cases across both
  new files; `smbios_perf_test`/`smbios_mock_test` passing exactly as the
  established baseline; `smbios_integration_test` failing cleanly with
  assertion failures, not a crash, on this unprivileged, no-native-source
  macOS sandbox - the same established baseline every prior task in this
  milestone has recorded, not a regression). Windows/Linux CI confirmation
  is, as throughout this milestone, deferred to the next real CI push - not
  verifiable locally.
