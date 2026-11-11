# Story 01.0 - `libsmbios` Install Rules and Whole-Archive-Safe Package Export

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Give `libsmbios` (and the per-OS/legacy static libraries it `PUBLIC`
links) its first `install()` rules, then export a CMake package config
(`libsmbiosConfig.cmake`) that bakes the whole-archive link requirement
(`-force_load`/`/WHOLEARCHIVE`/`--whole-archive`) into the exported
`libsmbios::libsmbios` target as an `INTERFACE` link option - closing
[docs/adr/0004-package-manager-distribution.md](/docs/adr/0004-package-manager-distribution.md)'s
named gap: "nothing in `libsmbios`'s own build output currently forces the
whole-archive link onto consumers automatically." This story does **not**
publish `libsmbios` via any package manager (see the milestone's own
Non-goals) - it only makes the library safely consumable by anything that
links its install output directly (a future `find_package(libsmbios)`
consumer, and Story 03.0's GitHub Releases tarball).

## Tasks

| #  | Document                                                              | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Install rules for libsmbios and its dependencies](01-install-rules-for-libsmbios-and-dependencies.md) | ⬜ Not started | 02     |
| 02 | [Exported package config and whole-archive link](02-exported-package-config-and-whole-archive-link.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- **No change to `libsmbios`'s or any per-OS/legacy library's existing
  public header paths or namespace.** This story adds build-system
  metadata (install destinations, an exported target, a package config) -
  it is not a `tables/`-reorg-style source-relocation story.
- **`libsmbios` stays a static archive.** This story does not change
  `libsmbios` (or `sys_interface`/`libsmbios_win_legacy`/
  `libsmbios_posix_legacy`/`libsmbios_win`/`libsmbios_macos`/
  `libsmbios_linux`) to `SHARED`; `smbios_c` remains the one `SHARED`
  target in this project, for the reason its own `CMakeLists.txt`
  documents (a whole-archive-safe static consumer still needs the
  `INTERFACE_LINK_OPTIONS` fix this story adds; a shared `libsmbios` would
  be a materially bigger, unrelated change).
- **Exactly one exported target.** A consumer links `libsmbios::libsmbios`
  only; the per-OS/legacy libraries are exported too (so
  `install(EXPORT ...)` has a complete dependency graph to write into
  `libsmbiosTargets.cmake`), but are never meant to be linked directly by
  a consumer - `libsmbios`'s own `PUBLIC` dependency on them already
  propagates transitively through the one target a consumer touches.
- **The whole-archive `INTERFACE_LINK_OPTIONS` must reproduce, not
  reinvent, the existing three-way platform branch.** `smbios/src/CMakeLists.txt`,
  `smbios_c/CMakeLists.txt`, and the root `CMakeLists.txt`'s fuzzer target
  all already carry the identical `APPLE`/`MSVC`/else branch
  (`-force_load`/`/WHOLEARCHIVE`/`--whole-archive`); Task 02 must express
  that exact same logic as a generator expression baked into the exported
  target, not a fourth hand-written copy with its own subtly different
  behavior.
- **No new CMake minimum-version requirement.** The root `CMakeLists.txt`
  pins `cmake_minimum_required(VERSION 3.13)` (needed for
  `target_link_options`); Task 02's generator-expression approach must
  keep working at that floor, not silently require CMake 3.24's
  `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>` genex.

## Files or directories created or modified by this story

- `libsmbios/CMakeLists.txt`, `libsmbios/src/CMakeLists.txt` -
  `install()` rules and `install(EXPORT ...)`.
- `sys_interface/CMakeLists.txt`, `libsmbios_posix_legacy/CMakeLists.txt`
  or `libsmbios_win_legacy/CMakeLists.txt` (whichever the build selected),
  and `libsmbios_linux/CMakeLists.txt`/`libsmbios_macos/CMakeLists.txt`/
  `libsmbios_win/CMakeLists.txt` (whichever the build selected) - each
  gains `install()` rules so the exported target's transitive dependency
  graph is complete.
- `libsmbios/cmake/libsmbiosConfig.cmake.in` (new), `libsmbios/cmake/libsmbiosConfigVersion.cmake`
  generation (new) - see `plan.md`'s contract C1.
- No change to `smbios_c/CMakeLists.txt` or `smbios_c/smbios_c.pc.in`
  (milestone 0007's work) - this story is additive only.
