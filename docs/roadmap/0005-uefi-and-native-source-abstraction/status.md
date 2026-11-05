# Milestone 0005 - Native Source Abstraction and UEFI Support - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                               | Status         | Tests |
|-------|---------------------------------------|----------------|-------|
| 01.0  | `ISmbiosSource` strategy interface   | ⬜ Not started | -     |
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
