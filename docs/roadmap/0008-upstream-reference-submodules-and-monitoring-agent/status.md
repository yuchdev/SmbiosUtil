# Milestone 0008 - Upstream Reference Submodules and Monitoring Agent - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                       | Status         | Tests |
|-------|-----------------------------------------------|----------------|-------|
| 01.0  | Git submodule conversion and license isolation | ⬜ Not started | -     |
| 02.0  | Submodule update and pinning workflow          | ⬜ Not started | -     |
| 03.0  | Upstream change-monitoring agent               | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request, following directly from the symlink
  mis-assessment incident during milestone 0005's planning (the macOS
  reference was assumed empty because `find` doesn't follow symlinks by
  default - a real submodule would have shown up in `git status` as
  present, making that mistake harder to make).
- **2026-11-03, user decision on Windows:** no Windows submodule in this
  milestone. No canonical, actively-maintained "dmidecode for Windows"
  port is known to exist; inventing one or guessing a URL was explicitly
  rejected as an option. Windows's reference is Microsoft's own
  `GetSystemFirmwareTable`/WMI documentation (already the basis for
  `win_bios.hpp`), watched by Story 03.0's agent as a documentation page,
  not a git submodule. Revisit only if a credible port is identified later
  - see `plan.md`'s "Confirmed upstreams" section.
- Confirmed submodule URLs (verified via `git remote -v` against the
  existing symlinked checkouts, not guessed):
  - `dmidecode_linux` → `https://github.com/mirror/dmidecode.git`
  - `dmidecode_macos` → `https://github.com/acidanthera/dmidecode.git`
- **License note carried forward from this discovery:** both upstreams
  are GPL-2.0-or-later. This is why Story 01.0 treats build-exclusion as a
  structural requirement (R2), not just a comment in CLAUDE.md.
