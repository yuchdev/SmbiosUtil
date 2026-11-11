# 01 - Portfile and port vcpkg.json

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Write `ports/smbios-c/portfile.cmake` and `ports/smbios-c/vcpkg.json` -
the port itself - wrapping `smbios_c/CMakeLists.txt`'s existing,
milestone-0007-verified `install()` rules.

## Files

- **Create** `ports/smbios-c/vcpkg.json`:
  ```jsonc
  {
    "name": "smbios-c",
    "version": "1.0.0",
    "description": "C ABI shim over libsmbios for SMBIOS/DMI firmware table parsing",
    "homepage": "https://github.com/<org>/SmbiosSdk",
    "license": "MIT",
    "dependencies": [
      "boost-iostreams",
      "boost-date-time"
    ]
  }
  ```
  (the `"version"` here tracks `smbios_c/CMakeLists.txt`'s own
  `SMBIOS_C_VERSION` - confirm during implementation whether to generate
  this file's `"version"` field from that CMake variable at release time,
  or keep them two manually-synchronized values; either way, record the
  decision in `status.md` per the milestone's "no shared versioning
  policy" constraint. `boost-program-options`/`boost-test` - needed by
  the root `vcpkg.json` for the `smbios` CLI and test suites - are
  deliberately **not** listed here: `smbios_c` itself does not depend on
  either.)
- **Create** `ports/smbios-c/portfile.cmake`:
  ```cmake
  vcpkg_from_git(
      OUT_SOURCE_PATH SOURCE_PATH
      URL https://github.com/<org>/SmbiosSdk.git
      REF <tag-or-commit-matching-vcpkg.json's-version>
      SHA512 <computed-at-release-time>
  )

  vcpkg_cmake_configure(
      SOURCE_PATH "${SOURCE_PATH}"
      OPTIONS
          -DSMBIOS_BUILD_EXAMPLES=OFF
  )

  vcpkg_cmake_install()
  vcpkg_cmake_config_fixup(PACKAGE_NAME smbios_c) # if/once a CMake package
                                                    # config is added for
                                                    # smbios_c itself - see
                                                    # Constraints below
  vcpkg_fixup_pkgconfig()

  file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
  vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
  ```
  (illustrative - `vcpkg_from_git` vs. a different acquisition helper,
  and whether `smbios_c` needs its own CMake package config in addition
  to `smbios_c.pc`, are implementation decisions; see Constraints.)

## Functions and Classes

N/A - packaging metadata only.

## Constraints

- **The portfile configures and installs the whole project (root
  `CMakeLists.txt`), not a `smbios_c`-only subtree build**, since
  `smbios_c` is not an independently-buildable CMake project today (it is
  a subdirectory of the root `CMakeLists.txt`, which also configures
  `libsmbios`, the per-OS/legacy libraries, `smbios` the CLI, and
  `tests/`). `-DSMBIOS_BUILD_EXAMPLES=OFF` (and, if useful,
  skipping `tests/` - confirm whether `tests/CMakeLists.txt` needs its own
  opt-out flag, or whether vcpkg's own `vcpkg_cmake_install()` only
  installs targets with `install()` rules regardless of what else got
  built) keeps the port's build footprint to roughly what it needs.
  Whether this is acceptable or whether `smbios_c` should instead gain
  its own standalone top-level `CMakeLists.txt` (a bigger, separate
  restructuring this story does **not** take on) is a decision to record
  in `status.md`, not silently resolve by trial and error.
- **`vcpkg_fixup_pkgconfig()` must not change `smbios_c.pc`'s contents or
  location** relative to what milestone 0007 already produces and
  verified (`lib/pkgconfig/smbios_c.pc`) - the portfile adapts to the
  existing install layout, the install layout does not change to suit
  the portfile.
- **No `CMakeLists.txt` edits anywhere in this repository** - a vcpkg
  port wraps the existing build unmodified; if the portfile reveals a
  real gap (e.g. `smbios_c` genuinely needs its own package config for
  `vcpkg_cmake_config_fixup()` to work), that gap is a named follow-up
  task in this story, not a silent edit folded into "just the port."
- `SHA512`/`REF` in `portfile.cmake` are only fillable once a real tagged
  commit exists to point at - record the exact tag/release procedure in
  `status.md` at implementation time.

## Tests

- Covered by Story 02.0's Task 03 - a clean-room `vcpkg install
  smbios-c` (through the registry Task 02 builds) is the real
  verification for this task, not a standalone local check.

## Success criteria

- [ ] `vcpkg install smbios-c --overlay-ports=ports/` (a local,
      not-yet-registry-published smoke check) succeeds on at least one
      platform, installing `smbios_c`'s library, headers, and
      `smbios_c.pc`.
- [ ] `ports/smbios-c/vcpkg.json`'s declared dependencies exactly match
      what `smbios_c`'s own CMake build actually needs (no
      over-declaration of `boost-program-options`/`boost-test`, no
      missing dependency causing a configure failure).
- [ ] The portfile's own CMake options (`-DSMBIOS_BUILD_EXAMPLES=OFF`
      etc.) are documented inline with a one-line rationale each, per
      this project's own CMake-comment convention (see root
      `CMakeLists.txt`'s existing option comments for the house style).
