# 01 - Install rules for `libsmbios` and its dependencies

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add `install()` rules - the first `libsmbios` has ever had - for the
`libsmbios` target itself, `sys_interface` (header-only), whichever legacy
flavor the current platform's root `CMakeLists.txt` branch selected
(`libsmbios_win_legacy` or `libsmbios_posix_legacy`), and whichever per-OS
library it selected (`libsmbios_win`/`libsmbios_macos`/`libsmbios_linux`,
or none on other `*nix`), all under one named CMake export set so Task 02
has a complete dependency graph to generate `libsmbiosTargets.cmake` from.

## Files

- **Modify** `libsmbios/src/CMakeLists.txt`:
  ```cmake
  install(TARGETS libsmbios
      EXPORT libsmbiosTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin
  )
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/libsmbios
      DESTINATION include
  )
  ```
  (header destination placeholder - confirm during implementation whether
  milestone 0010's `tables/`/`formatters/` subdirectories need separate
  `DIRECTORY` entries or are already covered by this one recursive copy.)
- **Modify** `sys_interface/CMakeLists.txt`,
  `libsmbios_win_legacy/CMakeLists.txt` or
  `libsmbios_posix_legacy/CMakeLists.txt` (platform-selected), and
  `libsmbios_win/CMakeLists.txt`/`libsmbios_macos/CMakeLists.txt`/
  `libsmbios_linux/CMakeLists.txt` (platform-selected, if any): each gains
  an `install(TARGETS <target> EXPORT libsmbiosTargets ...)` of its own,
  joining the same named export set, plus an `install(DIRECTORY ...
  DESTINATION include)` for its own public headers (`smbios/legacy/...`,
  `smbios/sys_interface/...`, etc., per each library's own
  `include/smbios/<segment>/` convention).
- **No modification** to any library's source files or public headers -
  this task is build-system-only.

## Functions and Classes

N/A - build configuration only.

## Constraints

- Every library the root `CMakeLists.txt`'s platform branch actually
  builds for the current configuration must join the **same** export set
  name (`libsmbiosTargets`) so Task 02 can write one `install(EXPORT ...)`
  call, not one per library.
- Header installation must preserve each library's existing
  `#include <smbios/<segment>/...>` / `#include <libsmbios/...>` path
  convention exactly (CLAUDE.md's Architecture section) - a consumer's
  `#include` lines must be identical whether building from this repo's
  source tree or from an installed package.
- `sys_interface` is header-only (an `INTERFACE` library per its own
  `CMakeLists.txt`) - its `install(TARGETS ...)` form differs from the
  other, compiled libraries' (no `ARCHIVE`/`LIBRARY`/`RUNTIME`
  destination; `INCLUDES DESTINATION` only). Get this right, not copied
  verbatim from a compiled target's install rule.
- Does not touch `smbios` (the CLI) or `smbios_c`'s own existing
  `install()`/build rules.

## Tests

- Build verification only - this task's test is "does `cmake --install`
  produce the expected `lib/`, `bin/`, and `include/` layout," covered by
  Task 03 alongside Task 02's export-config verification.

## Success criteria

- [ ] `cmake --install <build-dir> --prefix <scratch-dir>` (after a normal
      `cmake --preset <os>` + build) populates `<scratch-dir>/lib`,
      `<scratch-dir>/bin` (Windows DLL/import-lib case, if applicable),
      and `<scratch-dir>/include` with `libsmbios` and every
      platform-selected dependency library's artifacts and headers, on
      Windows, Linux, and macOS.
- [ ] Every existing `#include` path used by `smbios`, `smbios_c`, and
      `tests/*` against these libraries' headers still resolves
      unchanged - this task adds installability, it does not move a
      single header.
- [ ] No change to `smbios`'s or `smbios_c`'s own build output or test
      results (`ctest --test-dir build/<preset>` stays green, unaffected).
