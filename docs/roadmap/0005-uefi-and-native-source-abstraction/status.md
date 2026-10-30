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
