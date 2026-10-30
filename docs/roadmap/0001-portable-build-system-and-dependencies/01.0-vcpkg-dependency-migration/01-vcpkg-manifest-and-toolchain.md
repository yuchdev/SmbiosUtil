# 01 - vcpkg manifest and toolchain

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add a vcpkg manifest declaring every third-party dependency this project
currently resolves by hand, and document the toolchain-file invocation that
replaces `-DWINDOWS_BOOST_DIR=<path>` / `-DMY_BOOST_DIR=<path>` /
`-DBOOST_ROOT=<path>`.

## Files

- **Create** `vcpkg.json`:
  ```json
  {
    "name": "libbios",
    "version": "0.1.0",
    "builtin-baseline": "<pinned vcpkg commit sha>",
    "dependencies": [
      "boost-iostreams",
      "boost-program-options",
      "boost-test",
      "boost-date-time",
      "boost-functional",
      "boost-bind"
    ]
  }
  ```
- **Create** `vcpkg-configuration.json` pinning the registry baseline used
  above, so `builtin-baseline` is not the only place the commit is recorded.
- **Modify** top-level build docs (CLAUDE.md's "Build and test" section, or
  a new `docs/dev/building.md`) with the new invocation:
  ```
  cmake --preset default   # once Story 02.0 lands CMakePresets.json;
                            # until then: -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
  ```

## Functions and Classes

N/A - this task adds configuration files, not C++ source.

## Constraints

- `builtin-baseline` must be an explicit commit SHA from the vcpkg registry,
  not a branch name, so the dependency set is reproducible across machines
  and CI runs until deliberately bumped.
- Boost.Test is declared as `boost-test` (vcpkg's name for
  `unit_test_framework`); verify the port actually ships the
  `unit_test_framework` library component CLAUDE.md's test target links
  against, and record any `features` needed to get it.

## Success criteria

- [ ] `vcpkg install` (manifest mode, from repo root) resolves all six
      declared dependencies without manual intervention on a clean machine.
- [ ] `vcpkg.json` and `vcpkg-configuration.json` are committed and documented.
- [ ] The build documentation shows the exact toolchain-file command line
      that replaces every `-D*_BOOST_DIR` / `-DBOOST_ROOT` invocation
      CLAUDE.md currently documents.
