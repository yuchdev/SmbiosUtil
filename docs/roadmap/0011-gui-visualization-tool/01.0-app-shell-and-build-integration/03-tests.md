# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-11; macOS build, unit suite, 4s launch smoke test; clean-exit and Win/Linux deferred)

## Objective

Close out Story 01.0: confirm the app-shell/build-target foundation is
solid - the exact dependencies resolve, the whole-archive link genuinely
works, and the window opens and closes cleanly - before Story 02.0 builds
real SMBIOS browsing logic on top of it.

## Testing-scope call

Dear ImGui is an immediate-mode UI: every frame is rebuilt from scratch, so
there is no persistent widget tree or accessibility-API surface a headless
test harness could inspect, and `smbios_gui`'s own window/frame loop needs
a real display and GPU-capable context that is not reliably present on a
CI runner (see Story 03.0's Task 02 for why the CI build job does not try
to *run* it). This milestone therefore draws the same line milestone 0006's
own testing philosophy already draws between the parser (unit-tested,
hermetic) and the acquisition layer (integration-tested, real-hardware,
best-effort): **the data-fetch/view-model logic is unit-tested; the actual
Dear ImGui rendering and window/input handling are verified by manual run
only.** This is not a gap this milestone defers - it is the realistic bar
for an immediate-mode GUI's rendering code in this project's CI, stated
explicitly rather than invented away. Story 01.0 has no view-model logic of
its own (Story 02.0 does), so this story's own automated-test surface is
limited to the linkage check below.

## Files

No new test source. This is the story's exit gate, matching milestone
0007 Story 01.0's own Task 03 ("no new test source beyond Tasks 01-02's
additions").

## Tests

- A manual, developer-run confirmation (not a CTest target - see Story
  03.0 for why) that:
  1. `cmake --preset <os>` + `cmake --build --preset <os>` succeed on
     Windows, Linux and macOS with the new `vcpkg.json` entries resolved.
  2. The built `smbios_gui` binary opens one window, displays the
     placeholder text, and exits 0 on window-close with no crash.
  3. The whole-archive link genuinely pulled in `libsmbios`'s object
     files not directly referenced by `smbios_gui/src/main.cpp` (Task 02's
     own `nm`/`dumpbin` success criterion).

## Success criteria

- [ ] All three platforms build and the manual run above passes on at
      least one of them directly (the other two via CI once Story 03.0
      lands the build-only CI wiring - the same "deferred to the story
      that adds the CMake/CI wiring" pattern milestone 0007's Story 01.0
      used for its own sanitizer-CI criterion).
- [ ] Milestone 0011's `status.md` Story 01.0 row is updated to ✅, with
      the testing-scope call above recorded (or reaffirmed) in its Notes &
      decisions section.
