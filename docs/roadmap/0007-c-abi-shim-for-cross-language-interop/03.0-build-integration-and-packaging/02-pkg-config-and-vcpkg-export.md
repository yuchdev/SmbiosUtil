# 02 - pkg-config and vcpkg export

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Generate a `smbios_c.pc` pkg-config file at install time, so a non-CMake
C/FFI build (a Python `setup.py` extension, a Rust `build.rs` via the
`pkg-config` crate, a plain `Makefile`) can discover `smbios_c`'s include
path and link flags with `pkg-config --cflags --libs smbios_c`, and decide
whether `smbios_c` should be independently installable via vcpkg.

## Files

- **Create** `smbios_c/smbios_c.pc.in`:
  ```
  prefix=@CMAKE_INSTALL_PREFIX@
  libdir=${prefix}/lib
  includedir=${prefix}/include

  Name: smbios_c
  Description: C ABI for libbios SMBIOS parsing
  Version: @PROJECT_VERSION@
  Libs: -L${libdir} -lsmbios_c
  Cflags: -I${includedir}
  ```
- **Modify** `smbios_c/CMakeLists.txt`: `configure_file(smbios_c.pc.in
  smbios_c.pc @ONLY)` plus an `install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/smbios_c.pc DESTINATION
  lib/pkgconfig)`.

## Functions and Classes

N/A - packaging only.

## Constraints

- Decide, and record in `status.md`, whether `smbios_c` ships as part of
  this project's existing `vcpkg.json` manifest (consumer-only, per
  milestone 0001) or as a genuinely new, separately-publishable vcpkg
  *port* (`portfile.cmake` + `vcpkg.json` under a `ports/smbios-c/`
  layout) - the latter is a meaningfully bigger commitment (vcpkg
  registry submission, independent versioning) and may be deferred to a
  future milestone if the immediate goal is "usable via `pkg-config`
  today," not "installable via `vcpkg install smbios-c` today."

## Tests

- A manual check: `pkg-config --cflags --libs smbios_c` after a local
  `cmake --install` run produces correct, usable flags; compile and link
  a trivial external C program against them without referencing this
  repo's source tree directly.

## Success criteria

- [ ] `smbios_c.pc` is generated and installed correctly.
- [ ] `pkg-config --cflags --libs smbios_c` works against an installed
      build.
- [ ] The vcpkg-port-vs-manifest-only decision is made and recorded.
