# 03 - CI workflow matrix

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (presets), Task 02 (clean target flags)

## Objective

Add `.github/workflows/ci.yml` running a `{windows-latest, ubuntu-latest,
macos-latest}` matrix: install/cache vcpkg, configure with the matching
preset from Task 01, build, and run what the test suite can run
unprivileged.

## Files

- **Create** `.github/workflows/ci.yml`:
  - checkout, setup vcpkg (via `lukka/run-vcpkg` or an equivalent pinned
    action, or a manual clone-and-bootstrap step - decide and record in
    this task's success criteria),
  - `cmake --preset <os>` / `cmake --build --preset <os>`,
  - run `smbios_functional_test` where it does not require elevated
    privileges; where it does (reading the real machine's SMBIOS table),
    either run it under `sudo`/an elevated CI runner step, or mark that job
    build-only with a comment pointing at milestone 0006 as where this gets
    fixed properly.

## Functions and Classes

N/A.

## Constraints

- Pin third-party GitHub Actions by commit SHA or exact version tag, not a
  floating `@main`/`@v1` branch reference.
- Cache the vcpkg binary cache (`vcpkg export`/`VCPKG_BINARY_SOURCES` or the
  setup action's built-in caching) keyed on `vcpkg.json`'s contents, so CI
  does not rebuild Boost from source on every run.

## Success criteria

- [ ] A push to any branch triggers all three OS jobs.
- [ ] All three jobs build `smbios` and `smbios_util` successfully.
- [ ] At least a build-only (if not full execution) signal exists per OS for
      `smbios_functional_test`; the workflow comments explain any privilege
      gap and link to milestone 0006.
- [ ] A second CI run on an unchanged `vcpkg.json` is measurably faster than
      the first (cache hit), confirmed by comparing job logs.
