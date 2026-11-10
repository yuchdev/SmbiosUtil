# 01 - Preset and root CMake confirmation

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm, on a real checkout of each of the three `CMakePresets.json`
presets, that `smbios_gui` configures and builds with no preset-specific
change needed - and if one *is* needed (most likely: a system package
GLFW's vcpkg port expects to find on a fresh Linux image, e.g. X11/Wayland
development headers), make the smallest change that fixes it and document
why.

## Files

- **Modify** (only if a real gap is found - this task may conclude with no
  file changes at all beyond what Story 01.0 already made):
  - `CMakePresets.json` - only if a preset genuinely needs a new
    `cacheVariable` (e.g. an explicit `VCPKG_TARGET_TRIPLET` or
    `CMAKE_PREFIX_PATH` hint `glfw3`'s port documentation calls for on one
    platform, confirmed by a real failed-then-fixed configure, not assumed
    in advance).
  - Root `CMakeLists.txt` - only if `smbios_gui` needs platform-specific
    link flags beyond what Story 01.0, Task 02 already wrote (e.g. `X11`/
    `dl`/`pthread` on Linux, which GLFW's own vcpkg port usually declares
    as transitive dependencies already - confirm before adding anything
    redundant).

## Functions and Classes

None - this is a build-configuration confirmation task, not a code task.

## Constraints

- **Confirm, don't guess.** Every change this task makes must be traced to
  a real, reproduced configure/build failure on the platform it fixes -
  this task must not add speculative `find_package`/link flags "just in
  case." If all three presets build cleanly with zero changes beyond
  Story 01.0's own, that is this task's successful outcome, recorded as
  such.
- **No new preset.** `windows`/`linux`/`macos` (and their inherited
  `default`) are the only configure presets this project has; this task
  does not add a fourth.

## Tests

None of this task's own; [Task 03](03-tests.md) is the story's exit gate.

## Success criteria

- [ ] `cmake --preset windows && cmake --build --preset windows` succeeds
      from a clean checkout (no pre-existing `build/windows` directory),
      with `VCPKG_ROOT` set and nothing else.
- [ ] The same for `linux` and `macos`.
- [ ] Any change this task made is justified in `status.md` by the exact
      error it fixed, not by anticipation.
