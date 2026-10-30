# 01 - smbios_c CMake target

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add `smbios_c` as a new CMake target (shared and/or static, matching
whatever `smbios` itself already builds as per milestone 0001) depending
on `smbios`, built and tested through the same presets and CI matrix
milestone 0001 established.

## Files

- **Create** `smbios_c/CMakeLists.txt`:
  ```cmake
  add_library(smbios_c src/smbios_c.cpp)
  target_include_directories(smbios_c PUBLIC include)
  target_link_libraries(smbios_c PRIVATE smbios)
  set_target_properties(smbios_c PROPERTIES
      CXX_STANDARD 20
      C_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden)
  ```
  (exact visibility/export-macro handling - e.g. a generated
  `smbios_c_export.h` via CMake's `GenerateExportHeader` module - to be
  finalized during implementation; the goal is that only the intended
  `extern "C"` functions are exported from the shared library, not every
  C++ symbol `smbios_c.cpp` happens to pull in from `smbios`.)
- **Modify** root `CMakeLists.txt`: `add_subdirectory(smbios_c)`.

## Functions and Classes

N/A - build configuration only.

## Constraints

- `smbios_c` links `smbios` `PRIVATE`, not `PUBLIC` - a consumer of
  `smbios_c` must never need to also link or include `smbios`'s C++
  headers; the C ABI is the complete interface.
- `CXX_VISIBILITY_PRESET hidden` (or the per-platform equivalent -
  `/MP`-style export control on MSVC differs from GCC/Clang visibility
  attributes) ensures a shared `smbios_c` build does not accidentally
  export C++ symbols from `smbios` through it.

## Tests

- Build verification only - this task's test is "does the target build,"
  covered by Task 03.

## Success criteria

- [ ] `smbios_c` builds via `cmake --preset <os>` (milestone 0001) on
      Windows, Linux and macOS.
- [ ] `nm -D` / `dumpbin /exports` (as appropriate per platform) on a
      shared `smbios_c` build shows only the intended `smbios_*` C
      functions - no leaked C++ symbols from `smbios`.
