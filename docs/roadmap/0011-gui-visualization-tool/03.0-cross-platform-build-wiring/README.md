# Story 03.0 - Cross-Platform Build Integration

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`smbios_gui` target existing); can proceed in parallel with Story 02.0

## Scope

Confirm `smbios_gui` builds cleanly through the existing
`CMakePresets.json`/vcpkg pipeline on all three platforms, and wire a
**build-only** CI step for it - no new CI job, no installer/bundling
pipeline. This story is intentionally the smallest of the three: milestone
0001 already did the heavy lifting (vcpkg toolchain file, per-OS presets,
CI matrix), and Story 01.0's `vcpkg.json`/CMake changes are additive, so
this story's job is confirmation and CI wiring, not new infrastructure.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Preset and root CMake confirmation](01-preset-and-root-cmake-confirmation.md) | ⬜ Not started | 02     |
| 02 | [CI build-only wiring](02-ci-build-only-wiring.md)                    | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                 | ⬜ Not started | -      |

## Key constraints

- **No new preset, no new build mode.** `smbios_gui` builds through the
  exact same `windows`/`linux`/`macos` presets every other target already
  uses (`CMakePresets.json` is not modified by this story) - Story 01.0's
  own `add_subdirectory(smbios_gui)` line is the only wiring needed for
  the target to exist in the graph; this story only *confirms* that and
  closes any platform-specific gap it finds (e.g. a missing system package
  GLFW needs on a fresh Linux CI image - X11/Wayland development headers,
  which vcpkg's own `glfw3` port declares as its own dependencies and
  installs, but confirm this rather than assume it).
- **CI builds it; CI does not run it.** A GUI needs a real display and a
  GPU-capable context. GitHub-hosted runners are not guaranteed to provide
  either reliably (`ubuntu-latest`/`windows-latest`/`macos-latest` have no
  attached display by default, and software OpenGL fallbacks like Mesa's
  llvmpipe are not installed by default either). This story therefore adds
  `smbios_gui` to the existing `build` job's **build** step only (the step
  that already runs `cmake --build --preset ${{ matrix.preset }}` and
  would fail the whole job if `smbios_gui` failed to compile or link) -
  never a "run `smbios_gui` and screenshot it" CI step. This is this
  milestone's own testing-scope call for its build/CI concern, matching
  the one `plan.md`'s Non-goals and Story 01.0/02.0's own tests tasks
  already state for the rendering code itself.
- **No installer/bundling pipeline.** No `.app`/`.dmg`/`.msi`/AppImage
  packaging, no code signing, no application icon - explicitly out of
  scope (see Non-goals below), matching the task's own instruction that
  this is "not a full installer pipeline... unless judged essential," and
  it is judged *not* essential: a developer-run binary under
  `smbios_gui/bin/` is sufficient for this milestone's diagnostic-tool
  scope (ADR 0003's own framing).
- **No change to the existing `smbios_cli_*`/`smbios_example_*` must-pass
  `ctest -R` regexes.** This story adds no new `ctest`-registered test of
  its own beyond Story 02.0's `gui_view_model_test.cpp` cases, which are
  already covered by the existing `smbios_unit_test` pattern match in
  `.github/workflows/ci.yml`'s `-R "smbios_unit_test|..."` filters - no
  regex edit needed.

## Non-goals

- Application packaging/installers of any kind (`.app`/`.dmg`/`.msi`/
  AppImage), code signing, or an application icon.
- A CI step that launches `smbios_gui` itself (headless/virtual-display
  tooling such as `xvfb-run` plus a software GL renderer is a legitimate
  future option if a real need for it appears, but is not scoped into
  this milestone - see Open questions).
- Any change to `vcpkg-configuration.json` or the `builtin-baseline`
  commit (Story 01.0, Task 01 already covers the one `vcpkg.json` edit
  this milestone needs).

## Open questions

- Should a future milestone add an `xvfb-run` (Linux) / software-GL
  headless CI step that actually launches `smbios_gui` against a
  synthetic or empty table and checks for a clean exit? This would close
  the one remaining gap this story's "build-only" scope leaves open, but
  needs its own investigation into Windows/macOS equivalents (there is no
  single cross-platform `xvfb-run`) - left as a follow-up, not a
  requirement of this milestone.

## Files or directories created or modified by this story

- `.github/workflows/ci.yml` (the `build` job's existing build step
  already covers `smbios_gui` once Story 01.0 lands
  `add_subdirectory(smbios_gui)` - this story's own edit, if any, is
  limited to confirming/documenting that, and to any platform-specific
  package install step a fresh CI image turns out to need for GLFW's
  system dependencies).
- `docs/cli-usage.md`/root `CLAUDE.md` gain a one-line pointer to
  `smbios_gui` as this milestone's own documentation footprint, matching
  how milestone 0007 added a one-line `smbios_c` mention to the same
  files rather than a new doc of its own.
