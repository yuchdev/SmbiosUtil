# 02 - CI build-only wiring

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Make sure `.github/workflows/ci.yml`'s existing `build` job's `Build` step
(`cmake --build --preset ${{ matrix.preset }}`) actually fails the job if
`smbios_gui` fails to compile or link, on all three OS matrix entries - and
add nothing that tries to *run* `smbios_gui`, per this story's own
testing-scope call.

## Files

- **Modify** `.github/workflows/ci.yml` only if Task 01 found a
  platform-specific system package `glfw3`'s vcpkg port needs that the CI
  image does not already have (e.g. an `apt-get install` step for X11/
  Wayland development headers on `ubuntu-latest`, ahead of the `Configure`
  step) - vcpkg's own binary-cache/port build step would otherwise fail
  the `Configure`/`Build` step anyway, so this is "does the existing job
  fail correctly and informatively," not new CI surface.
- No other change: the `Build` step already runs unconditionally for
  every target `add_subdirectory()`'d from the root `CMakeLists.txt`, and
  Story 01.0 added `smbios_gui` to that list - so no new CI step, matrix
  entry, or job is needed to make CI build it.

## Functions and Classes

None.

## Constraints

- **Build-only. No run step.** Do not add a step that executes
  `smbios_gui` (not even with a short timeout, not even just to check the
  exit code) - see the story README's Key constraints for the display/GPU
  reasoning. If a future milestone wants a headless run check, that is a
  new, separately-scoped CI investigation (see the story README's Open
  questions), not a one-line addition here.
- **Do not add `smbios_gui` to any existing `ctest -R` filter.** It is not
  a `ctest`-registered target (it is a plain executable, like `smbios`
  itself) - the `gui_view_model_test.cpp` cases from Story 02.0 are
  already covered by the existing `smbios_unit_test` filter entries with
  no edit needed.
- **Mirror the existing job's `allow_build_failure: false` posture.**
  `smbios_gui` failing to build is treated exactly like any other target
  failing to build (`matrix.allow_build_failure` is already `false` for
  all three OSes in the `build` job) - this task does not carve out a
  softer failure mode for it.

## Tests

None of this task's own; [Task 03](03-tests.md) is the story's exit gate.

## Success criteria

- [ ] A CI run on a real push shows `smbios_gui`'s object files/link step
      in the `Build` step's own log output, on all three OSes.
- [ ] Deliberately breaking `smbios_gui/src/main.cpp` (e.g. a syntax
      error) in a throwaway branch/PR, pushed once during this task's own
      verification and then reverted, fails the `build` job on all three
      OSes - proving this is a genuine hard gate, not a step CI silently
      skips.
- [ ] No CI step attempts to execute `smbios_gui`.
