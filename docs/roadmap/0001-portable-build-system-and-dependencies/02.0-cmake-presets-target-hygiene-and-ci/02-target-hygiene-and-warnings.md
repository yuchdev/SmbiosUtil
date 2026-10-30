# 02 - Target hygiene and warnings

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Consolidate the per-compiler flag blocks in the root `CMakeLists.txt`
(Clang `-stdlib=libc++`, GCC `-pthread -fPIC`, Windows `/EHsc`) behind
`target_compile_options`/`target_link_libraries` on the `smbios` and
`smbios_util` targets specifically, instead of global `CMAKE_CXX_FLAGS`, and
turn on `-Wall -Wextra` (`/W4` on MSVC) as a baseline so milestone 0002's
warnings-as-errors task has a clean starting point.

## Files

- **Modify** root `CMakeLists.txt`, `smbios/CMakeLists.txt`,
  `smbios_util/CMakeLists.txt`: replace any `set(CMAKE_CXX_FLAGS ...)`
  global mutation with `target_compile_options(<target> PRIVATE ...)` scoped
  to `smbios` and `smbios_util`.

## Functions and Classes

N/A.

## Constraints

- This task adds warning **flags** only (`-Wall -Wextra` / `/W4`); it does
  **not** turn them into errors yet (`-Werror`/`/WX`) and does not fix any
  warning it surfaces - that is milestone 0002's Story 01.0, Task 02
  (warnings-as-errors and sanitizers), which depends on this task.
- `gnu++1y`/C++11 stays the configured standard in this milestone; raising
  it to C++20 is milestone 0002's Task 01 - do not combine the two changes
  here even though both touch `CMakeLists.txt`.

## Success criteria

- [ ] No target-wide flag is set via a global `CMAKE_CXX_FLAGS`/
      `add_compile_options` call; every flag is attached to the `smbios` or
      `smbios_util` target explicitly.
- [ ] `-Wall -Wextra` (or `/W4`) is enabled and the build still succeeds
      (warnings are allowed to appear in the build log at this stage - they
      are milestone 0002's input, not this task's problem to fix).
