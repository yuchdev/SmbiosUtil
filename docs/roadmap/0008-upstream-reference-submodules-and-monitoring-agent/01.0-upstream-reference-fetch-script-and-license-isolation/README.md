# Story 01.0 - Upstream Reference Fetch Script and License Isolation

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

This story adds a tracked manifest (`scripts/upstream_references.json`)
and a cross-platform fetch script
(`scripts/fetch_upstream_references.py`). Together they reproducibly
populate the git-ignored `c-versions/dmidecode_linux` and
`c-versions/dmidecode_macos` with full clones of each upstream's
default-branch HEAD (R1).

It also extends the structural guard (R2) so that it fails loudly in
three cases:
- `c-versions/` is referenced from a build file or an `#include`;
- a `.gitmodules` entry appears;
- anything under `c-versions/` becomes tracked.

The guard enforces the GPL-isolation constraint, which is what makes it
safe to keep these references on disk at all.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Fetch script and manifest](01-fetch-script-and-manifest.md)            | ✅ Complete    | 02     |
| 02 | [Build isolation guard](02-build-isolation-guard.md)                   | ✅ Complete    | 03     |
| 03 | [Tests](03-tests.md)                                                   | ⬜ Not started | -      |

## Key constraints

- **No GPL code or text is tracked in any form.** That rules out a
  submodule, a `.gitmodules` file, a vendored copy, or a pasted excerpt.
  The `.gitignore` line `/c-versions/` is load-bearing and stays.
- This story touches no file under this project's own build-product
  directories: `libsmbios/`, `smbios/` (the CLI), `sys_interface/`,
  `libsmbios_win/`, `libsmbios_macos/`, `libsmbios_linux/`,
  `libsmbios_win_legacy/`, `libsmbios_posix_legacy/`, `smbios_c/` (see
  CLAUDE.md).
- **The script never deletes or replaces anything.** Developers may
  still have machine-local symlinks at `c-versions/dmidecode_*` pointing
  at their own checkouts (`~/Projects/dmidecode_linux`/`dmidecode_macos`).
  The script warns about these and skips them, so they keep working.

## Files or directories created or modified by this story

- `scripts/upstream_references.json` (new, tracked): metadata only.
- `scripts/fetch_upstream_references.py` (new).
- `c-versions/dmidecode_linux`, `c-versions/dmidecode_macos`: populated
  locally by the script and never tracked.
- `scripts/check_cversions_isolation.py` (new) and an early step in
  `.github/workflows/ci.yml`: the extended isolation guard.
- `CLAUDE.md`: the `c-versions/` sentence.
- `.gitignore`: comment above `/c-versions/` only. The rule itself is
  unchanged.
