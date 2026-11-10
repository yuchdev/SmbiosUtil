# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Prove the self-hosted vcpkg port works end to end, from a clean-room
consumer's perspective, closing out Story 02.0 - directly re-running the
verification ADR 0004's own Validation/Rollout section specifies: "a
trivial external C/C++ program, with no reference to this repo's source
tree, built via `vcpkg install smbios-c` (through the overlay/registry)
... run successfully against real SMBIOS data on at least Linux and
macOS (Windows once the port's own triplet support is confirmed)."

This is a **packaging-correctness verification task, not a unit test** -
no Boost.Test case is added; this runs as its own CI job against a
scratch consumer project.

## Files

- **Create** a scratch consumer fixture (location decided during
  implementation, e.g. `tests/packaging/smbios_c_vcpkg_consumer/`,
  built only from its own CI step, not wired into the main `ctest`
  suite): a `vcpkg.json` declaring `"smbios-c"` as a dependency, a
  `vcpkg-configuration.json` with the registry entry from Task 02, and a
  trivial `.c`/`.cpp` program calling `smbios_open()`/iterating
  entries/`smbios_close()` - structurally identical in spirit to
  `tests/c_abi/smbios_c_example.c` (milestone 0007), but built via `vcpkg
  install` + `find_package(smbios_c CONFIG)` or `pkg-config`, never via
  this repo's own `CMakeLists.txt`.
- **Modify** `.github/workflows/ci.yml` (or a new workflow): a job that
  checks out *only* the scratch fixture (or a synthetic copy of it with
  no access to this repo's source tree) plus a vcpkg installation, runs
  `vcpkg install`, builds and runs the fixture, on Linux, macOS, and
  Windows.

## Success criteria

- [ ] `vcpkg install` resolves and builds `smbios-c` from this project's
      self-hosted registry (Task 02), with no `--overlay-ports` flag and
      no manual copy of this repo's source tree into the consumer
      project.
- [ ] The fixture compiles, links, and runs successfully against real
      SMBIOS data (or degrades to "no table found," per `smbios_open()`'s
      documented non-throwing behavior) on Linux and macOS; Windows
      verified once the port's own triplet (`x64-windows` or similar) is
      confirmed to build.
- [ ] `nm -gU` (or the Windows/Linux equivalent) re-run against the
      vcpkg-built artifact specifically - not just this project's own
      preset build - confirming the export allowlist
      (`smbios_c.exported_symbols`/`smbios_c.version`) survives vcpkg's
      own build invocation unchanged (ADR 0004's own Validation/Rollout
      item, repeated here verbatim as this task's own gate).
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems.
- [ ] This story's and the milestone's `status.md` are updated.
