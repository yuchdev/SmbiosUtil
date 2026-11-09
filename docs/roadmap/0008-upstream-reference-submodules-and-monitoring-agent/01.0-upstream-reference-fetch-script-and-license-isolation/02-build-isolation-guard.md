# 02 - Build isolation guard

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Add a structural check that fails CI (and, ideally, a local pre-commit
hook) in any of these cases:
- `c-versions/` is referenced from a build file or an `#include`
  directive;
- any GPL upstream becomes tracked in this repository, whether as a
  submodule or as committed files under `c-versions/`.

This turns the GPL-isolation constraint from a comment into something that
actually blocks a mistake. It also absorbs the protective intent of the
superseded Story 02.0 Task 02 (CI pin-drift check). With no pins, the
remaining risk is something becoming tracked at all.

## Files

- **Create** `scripts/check_cversions_isolation.py`. It runs three checks
  and reports every hit before exiting non-zero.
  - **Build-reference check.** Scope: every build-product directory, which
    means `libsmbios/`, `smbios/` (the CLI), `sys_interface/`,
    `libsmbios_win/`, `libsmbios_macos/`, `libsmbios_linux/`,
    `libsmbios_win_legacy/`, `libsmbios_posix_legacy/`, and `smbios_c/`
    (see CLAUDE.md). Within that scope it flags the substring
    `c-versions` in either of these places:
    - any **non-comment** line of `CMakeLists.txt`, `*.cmake`, or
      `vcpkg.json` (a CMake `#` comment is ignored);
    - any **`#include` line** of `*.h`/`*.hpp`/`*.c`/`*.cpp`.
  - **Submodule check.** Fail if `.gitmodules` exists at the repo root
    with any `[submodule ...]` entry. An empty file is tolerated, but
    flagged as a warning.
  - **Tracking check.** Fail if `git ls-files c-versions` prints anything.
  - Print a clear `file:line: reason` for every violation, then exit
    non-zero.
- **Modify** `.github/workflows/ci.yml` (milestone 0001): add this check
  as an early, fast-failing step before the actual build. The checkout is
  a plain one, with no `submodules:` key.
- **Modify** (optional, if this project's `.claude/hooks/` mechanism
  supports it) a pre-commit-style hook that runs the same check locally.

## Functions and Classes

N/A - tooling only. Follow the existing `scripts/*.py` conventions
(docstrings, `argparse`, `main(argv) -> int`).

## Constraints

- **Provenance comments stay legitimate.** About 40 existing comments in
  `libsmbios/`, `libsmbios_linux/`, `libsmbios_macos/`, and
  `sys_interface/` cite the reference (e.g. `// Cross-checked against
  c-versions/dmidecode_linux/dmidecode.c`). A comment cannot link GPL code
  into a binary, so the C/C++ check matches `#include` lines only, not
  any mention. This follows plan.md R2's wording and status.md's Phase R
  audit note.
- **Scope excludes prose.** `docs/`, `CLAUDE.md`, and `.claude/`
  legitimately mention `c-versions/` and are outside the scan scope.
- **Prefer false negatives to false positives** for day-to-day friction.
  Keep the pattern specific (`c-versions`) rather than broader.
- **The guard never needs `c-versions/` to exist.** CI never runs the
  fetch script.

## Tests

- **Deliberately broken fixtures.** Run the script against each of these
  and confirm it fails with a clear message:
  - a throwaway `CMakeLists.txt` line referencing
    `c-versions/dmidecode_linux`;
  - a throwaway `#include "../../c-versions/dmidecode_linux/types.h"`;
  - a throwaway `.gitmodules` entry;
  - a force-added (`git add -f`) file under `c-versions/`.
- **Reverting each fixture** gives a clean pass.
- **The current tree passes**, including its existing provenance comments.

## Success criteria

- [ ] The guard script fails on each deliberate violation class
      (build reference, `#include`, `.gitmodules` entry, tracked
      `c-versions/` file) and passes on the current, clean tree.
- [ ] CI runs it as an early step.
