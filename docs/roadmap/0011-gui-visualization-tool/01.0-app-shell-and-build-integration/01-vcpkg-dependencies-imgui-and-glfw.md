# 01 - vcpkg dependencies: Dear ImGui and GLFW

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-11; macOS configure verified, Windows/Linux deferred to Story 03.0 CI)

## Objective

Add the two new third-party dependencies ADR 0003's Decision names (Dear
ImGui plus one windowing/GPU backend) to the existing `vcpkg.json` manifest,
resolved through the same `builtin-baseline` + `CMakePresets.json`
toolchain-file mechanism every current dependency (Boost) already uses -
no new dependency-resolution mechanism, per `plan.md`'s Key constraints.

## Files

- **Modify** `vcpkg.json`:
  ```jsonc
  {
    "name": "libbios",
    "version": "0.1.0",
    "builtin-baseline": "00c5775211f45cd08b37fce0484b4cb940e422ab",
    "dependencies": [
      "boost-iostreams",
      "boost-program-options",
      "boost-test",
      "boost-date-time",
      {
        "name": "imgui",
        "features": ["glfw-binding", "opengl3-binding"]
      },
      "glfw3"
    ]
  }
  ```
  The exact feature names must match whatever the pinned `builtin-baseline`
  commit's `imgui` port registers (`vcpkg search imgui` / the port's own
  `vcpkg.json` under the resolved baseline) - confirm them against the
  actual port rather than assuming this task's names are final; record any
  correction in `status.md`.

> **Amended 2026-11-11 (maintainer ruling after /pr-review, see `status.md` Notes & decisions):** the GUI build is **opt-in**. As built, `imgui` (features `glfw-binding`, `opengl3-binding`) and
`glfw3` live under a non-default vcpkg manifest feature `"gui"` in
`vcpkg.json`, not in the top-level `dependencies` array shown above. The
baseline pin is unchanged.

## Functions and Classes

None - this task is manifest-only, no C++ code.

## Constraints

- **Do not bump `builtin-baseline`.** Every other dependency in this
  project is pinned to the baseline commit `vcpkg.json` already records
  (also mirrored in `vcpkg-configuration.json`); add the two new
  dependencies against that same baseline. Bumping the baseline to pick up
  a newer `imgui`/`glfw3` port is a separate, deliberate decision - out of
  scope here unless the pinned baseline's port is missing a feature this
  story genuinely needs.
- **Static linking, matching Boost's existing posture.** `vcpkg.json` does
  not set a triplet or `VCPKG_TARGET_TRIPLET` override in this task; the
  default triplet vcpkg resolves per platform (already how Boost is
  consumed) applies unchanged. Do not introduce a `DVCPKG_TARGET_TRIPLET`
  flag that is not already part of the documented build invocation
  (root `CLAUDE.md`'s Build and test section).
- **`imgui`'s backend features, not a hand-written backend.** Request the
  port's own `glfw-binding`/`opengl3-binding` features (which install
  `imgui_impl_glfw.h`/`.cpp` and `imgui_impl_opengl3.h`/`.cpp` alongside the
  core library) rather than vendoring those backend files into this repo -
  they are part of the upstream Dear ImGui project under the same MIT
  license, and the vcpkg port already packages them.

## Tests

None of this task's own - a clean `cmake --preset <os>` configure (which
fails loudly if a dependency cannot be resolved) is this task's own
functional check; Task 02 is the first thing that actually links against
the resolved packages, and Task 03 is the story's test exit gate.

## Success criteria

- [ ] `cmake --preset windows` / `--preset linux` / `--preset macos` each
      configure cleanly with the two new dependencies present (no `#include`
      or link step needed yet - Task 02 does that).
- [ ] The resolved `imgui` port's actual feature names are confirmed against
      the pinned baseline (not assumed from this document) and corrected in
      `status.md` if they differ.
- [ ] `vcpkg-configuration.json` needs no change (it only records the
      baseline commit, which this task does not touch).
