# Story 01.0 - vcpkg Dependency Migration

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Introduce a vcpkg manifest (`vcpkg.json` + `vcpkg-configuration.json`)
declaring the four Boost components this project actually uses
(`boost-iostreams`, `boost-program-options`, `boost-test`,
`boost-date-time`, plus `boost-functional`/`boost-bind`/`boost-function`
transitively for the factory in `smbios_entry_factory.h`), then remove the
`WINDOWS_BOOST_DIR` / `MY_BOOST_DIR` / manual `BOOST_ROOT` path-pointing
logic from the root `CMakeLists.txt` in favor of vcpkg's toolchain file.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [vcpkg manifest and toolchain](01-vcpkg-manifest-and-toolchain.md) | ⬜ Not started | 02     |
| 02 | [Remove manual Boost paths](02-remove-manual-boost-paths.md)      | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                              | ⬜ Not started | -      |

## Key constraints

- Pin a vcpkg baseline commit in `vcpkg-configuration.json` so builds are
  reproducible; do not float on vcpkg's latest registry head.
- Keep Boost at ≥ 1.61 per the existing documented minimum (CLAUDE.md); if
  the pinned vcpkg baseline resolves a newer Boost, that is acceptable and
  expected - the floor is a minimum, not a pin.
- No source file under `smbios/` or `smbios_util/` changes in this task.

## Files or directories created or modified by this story

- `vcpkg.json` - new manifest at repo root.
- `vcpkg-configuration.json` - new, pins the vcpkg registry baseline.
- `CMakeLists.txt` (root) - removes `WINDOWS_BOOST_DIR`/`MY_BOOST_DIR`
  handling; `find_package(Boost ...)` now resolves through vcpkg's
  toolchain file instead.
- `docs/dev/` (or README) - updated build instructions showing the vcpkg
  toolchain-file invocation that replaces the old `-DMY_BOOST_DIR=<path>` /
  `-DWINDOWS_BOOST_DIR=<path>` instructions.
