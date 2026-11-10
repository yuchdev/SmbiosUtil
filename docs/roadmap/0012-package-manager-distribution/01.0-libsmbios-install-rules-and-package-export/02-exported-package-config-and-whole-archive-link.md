# 02 - Exported package config and whole-archive link

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Generate and install `libsmbiosConfig.cmake` + `libsmbiosConfigVersion.cmake`
so `find_package(libsmbios CONFIG)` works against the installed package,
with the exported `libsmbios::libsmbios` target's `INTERFACE_LINK_OPTIONS`
already carrying the platform-correct whole-archive flag - implementing
`plan.md`'s contract C1.

## Files

- **Create** `libsmbios/cmake/libsmbiosConfig.cmake.in`:
  ```cmake
  @PACKAGE_INIT@
  include("${CMAKE_CURRENT_LIST_DIR}/libsmbiosTargets.cmake")
  check_required_components(libsmbios)
  ```
- **Modify** `libsmbios/src/CMakeLists.txt` (or a new
  `libsmbios/cmake/CMakeLists.txt`, implementation's choice), adding the
  generator-expression-keyed whole-archive flag to the exported target
  before `install(EXPORT ...)` runs - the same three-way branch
  `smbios/src/CMakeLists.txt` already uses, expressed as an `INTERFACE`
  property instead of a `PRIVATE` one:
  ```cmake
  target_link_options(libsmbios INTERFACE
      $<$<PLATFORM_ID:Darwin>:LINKER:-force_load,$<TARGET_FILE:libsmbios>>
      $<$<CXX_COMPILER_ID:MSVC>:/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>>
      $<$<AND:$<NOT:$<PLATFORM_ID:Darwin>>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive>
  )
  ```
  (illustrative - confirm during implementation that `$<TARGET_FILE:libsmbios>`
  resolves correctly from a *consumer's* build, not just this repo's own;
  vcpkg/CMake export logic may require `$<TARGET_FILE:libsmbios::libsmbios>`
  or an install-relative path instead once the target is the *imported*
  one. This is exactly the kind of detail the ADR calls "real design-and-
  implementation work... out of this ADR's scope" - resolve it here, not
  by guessing in this planning doc.)
  ```cmake
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/libsmbiosConfigVersion.cmake
      VERSION ${LIBSMBIOS_VERSION}
      COMPATIBILITY SameMajorVersion
  )
  configure_package_config_file(
      cmake/libsmbiosConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/libsmbiosConfig.cmake
      INSTALL_DESTINATION lib/cmake/libsmbios
  )
  install(EXPORT libsmbiosTargets
      FILE libsmbiosTargets.cmake
      NAMESPACE libsmbios::
      DESTINATION lib/cmake/libsmbios
  )
  install(FILES
      ${CMAKE_CURRENT_BINARY_DIR}/libsmbiosConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/libsmbiosConfigVersion.cmake
      DESTINATION lib/cmake/libsmbios
  )
  ```
- A `LIBSMBIOS_VERSION` variable, set alongside (not replacing)
  `smbios_c/CMakeLists.txt`'s own independent `SMBIOS_C_VERSION` - the two
  are deliberately separate, per the milestone's Key constraints (no
  shared versioning policy is established by this milestone).

## Functions and Classes

N/A - build configuration only.

## Constraints

- The generator-expression branch must be verified to resolve correctly
  both in this repo's own build (where `libsmbios` is the real target)
  and from a downstream consumer's build (where `libsmbios::libsmbios` is
  an *imported* target) - these are not guaranteed to behave identically
  for `$<TARGET_FILE:...>`; this is the task's main implementation risk,
  flagged explicitly rather than hand-waved.
- `libsmbiosConfigVersion.cmake`'s `COMPATIBILITY SameMajorVersion` choice
  (vs. `AnyNewerVersion`/`ExactVersion`) should be revisited once this
  project adopts an actual versioning policy (currently informal, per the
  milestone's Key constraints) - record the choice and its rationale in
  `status.md`, don't leave it a silent default.
- No change to `smbios_c`'s own versioning (`SMBIOS_C_VERSION`) or its
  independent `smbios_c.pc` generation.

## Tests

- Covered by Task 03 (this story's own exit gate) - a clean-room consumer
  project is the real verification; this task's own local check is a
  manual `find_package(libsmbios CONFIG)` + `target_link_libraries(...
  libsmbios::libsmbios)` from a scratch CMake project pointed at the
  installed prefix via `CMAKE_PREFIX_PATH`, confirming configure succeeds
  and the whole-archive flag appears in the generated build's link
  command (`VERBOSE=1 cmake --build .` or the Ninja/MSBuild equivalent).

## Success criteria

- [ ] `find_package(libsmbios CONFIG REQUIRED)` succeeds from a scratch
      CMake project against the installed prefix, on Windows, Linux, and
      macOS.
- [ ] The consumer's own link command (inspected via a verbose build)
      shows the whole-archive flag applied to `libsmbios`'s archive file,
      with **zero** whole-archive-related lines in the consumer's own
      `CMakeLists.txt`.
- [ ] `libsmbiosConfigVersion.cmake` correctly rejects a
      `find_package(libsmbios 2.0 CONFIG)` request against a `1.x`
      install (or whatever major-version boundary is chosen) and accepts
      a same-major-version request.
