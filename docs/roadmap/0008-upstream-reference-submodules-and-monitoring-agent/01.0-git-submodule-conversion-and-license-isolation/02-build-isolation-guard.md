# 02 - Build isolation guard

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Add a structural check that fails CI (and, ideally, a local pre-commit
hook) if `c-versions/` is ever referenced from a build file or an
`#include` directive - turning the GPL-isolation constraint from a
comment into something that actually blocks a mistake.

## Files

- **Create** `scripts/check_cversions_isolation.py` (or a shell
  one-liner, if simpler - decide during implementation): greps every
  `CMakeLists.txt`, `vcpkg.json`, and `*.h`/`*.cpp`/`*.c` under every
  build-product directory (`libsmbios/`, `smbios/` [the CLI],
  `os_interface/`, `libsmbios_win/`, `libsmbios_macos/`, `libsmbios_linux/`,
  `libsmbios_win_legacy/`, `libsmbios_posix_legacy/`, and `smbios_c/` once
  it exists - see CLAUDE.md) for the literal substring
  `c-versions`, and fails with a clear message if any match is found.
- **Modify** `.github/workflows/ci.yml` (milestone 0001): add this check
  as an early, fast-failing step, before the actual build.
- **Modify** (optional, if this project's hooks mechanism from
  `.claude/hooks/` supports it) a pre-commit-style hook running the same
  check locally.

## Functions and Classes

N/A - tooling only.

## Constraints

- The check must itself exclude `docs/roadmap/` and `CLAUDE.md` from its
  grep scope (both legitimately reference `c-versions/` in prose, as this
  very milestone's own documents do) - scope the grep to source and build
  files only, exactly as the Files section above specifies.
- False positives are worse than false negatives here for day-to-day
  friction - keep the pattern specific (`c-versions`) rather than
  something broader that could flag an unrelated string.

## Tests

- A deliberately-broken test fixture (a throwaway `CMakeLists.txt` line
  referencing `c-versions/dmidecode_linux`) run through the script,
  confirming it fails with a clear message; removing that line and
  re-running confirms a clean pass.

## Success criteria

- [ ] The guard script correctly fails on a deliberate violation and
      passes on the current (clean) tree.
- [ ] CI runs it as an early step.
