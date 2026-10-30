# Story 03.0 - Build Integration and Packaging

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (the `smbios_c` target must exist)

## Scope

Wire `smbios_c` into the CMake build (milestone 0001's preset/CI
infrastructure) as a new target, generate a pkg-config file so any
build system (not just CMake) can discover and link it, and add vcpkg
port metadata so `smbios_c` is installable the same way this project's
own dependencies already are.

## Tasks

| #  | Document                                                    | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [smbios_c CMake target](01-smbios-c-cmake-target.md)              | ⬜ Not started | 02     |
| 02 | [pkg-config and vcpkg export](02-pkg-config-and-vcpkg-export.md)  | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                             | ⬜ Not started | -      |

## Key constraints

- `smbios_c` is a **separate** shared/static library target from
  `smbios` - a C caller links `smbios_c` only, never `smbios` directly
  (which would reintroduce the exact C++-ABI problem this milestone
  exists to avoid).
- No change to `smbios`/`smbios_util`'s existing CMake targets beyond
  adding `smbios_c` as a new sibling that depends on `smbios`.

## Files or directories created or modified by this story

- `smbios_c/CMakeLists.txt` (new).
- Root `CMakeLists.txt` - `add_subdirectory(smbios_c)`.
- `smbios_c/smbios_c.pc.in` (new, pkg-config template).
- `vcpkg.json` (milestone 0001) or a new port-specific manifest, if this
  project chooses to publish `smbios_c` as its own installable vcpkg
  component - decided during Task 02.
