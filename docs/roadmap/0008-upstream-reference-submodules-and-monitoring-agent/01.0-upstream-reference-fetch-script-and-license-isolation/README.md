# Story 01.0 - Git Submodule Conversion and License Isolation

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Replace the two symlinks under `c-versions/` with real git submodules
pinned to the confirmed upstream URLs, and add the structural guard (R2)
that fails loudly if either submodule is ever referenced from a build
file - the enforcement mechanism for the GPL-isolation constraint that
makes these submodules safe to carry at all.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Add confirmed submodules](01-add-confirmed-submodules.md)              | ⬜ Not started | 02     |
| 02 | [Build isolation guard](02-build-isolation-guard.md)                   | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                   | ⬜ Not started | -      |

## Key constraints

- This story is purely additive to repository structure - it does not
  touch any file under this project's own build-product directories
  (`libsmbios/`, `smbios/` [the CLI], `sys_interface/`, `libsmbios_win/`,
  `libsmbios_macos/`, `libsmbios_linux/`, `libsmbios_win_legacy/`,
  `libsmbios_posix_legacy/`, or `smbios_c/` once it exists - see
  CLAUDE.md).
- The existing machine-local symlinks
  (`~/Projects/dmidecode_linux`/`dmidecode_macos`) are the user's own
  working checkouts, outside this repository - this story does not delete
  or modify them; it only changes what `c-versions/dmidecode_linux`/
  `dmidecode_macos` resolve to *inside this repo* (a real submodule
  checkout instead of a symlink to a path outside the repo).

## Files or directories created or modified by this story

- `.gitmodules` (new).
- `c-versions/dmidecode_linux`, `c-versions/dmidecode_macos` - converted
  from symlinks to submodule checkouts.
- A new CI job or pre-commit hook (exact mechanism decided in Task 02) -
  the build-isolation guard.
