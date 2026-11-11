# Story 04.0 - GUI Test Tiers: CI-Safe and Local-Only

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (rendering code under test), Story 03.0 (the `gui-build` CI job and `<os>-gui` presets)

> **Scope extension, maintainer-authorized 2026-11-11 (verbatim):** "GUI app should include two specific groups of tests: 1 - that is possible to run of GH Actions, 2 - that is possible to run locally only, unsupported by Github Actions. Use definitions and CMake conditions. Make sure build and all tests pass locally and on CI."
>
> Where this conflicts with earlier milestone text, this story wins. That covers `plan.md`'s Non-goal "only the data-fetch/view-model logic layer is unit-tested", the Story 01.0/02.0 testing-scope calls ("Dear ImGui rendering ... verified by manual run only"), and Story 03.0's "CI never runs anything GUI". The new position is that CI runs the **CI-safe GUI tier** (`gui-ci`). CI still never runs the **local-only tier** (`gui-local`), and never runs the `smbios_gui` binary itself against a display. Pixel or visual-regression testing is still a non-goal.

## Scope

This story adds two automated test tiers for `smbios_gui`, separated by CMake conditions and compile definitions:

| Tier | CTest labels | Needs | Runs on CI | Classified as |
|------|--------------|-------|------------|---------------|
| **CI-safe** (`tests/gui/ci/`, `smbios_gui_ci_test`) | `gui;gui-ci` | Nothing. A Dear ImGui context with no window, no GL and no OS clipboard; synthetic tables only | Yes, in the `gui-build` job on all three OSes | Unit (hermetic, must pass, never skips) |
| **Local-only** (`tests/gui/local/`, `smbios_gui_local_test.*`) | `gui;gui-local` | A real display and GL context, the OS clipboard, the live SMBIOS table | Never. It is not configured, compiled or registered under CI | Integration (skips cleanly when a resource is missing) |

The tiers are run with `ctest -L gui-ci`, `ctest -L gui-local` or `ctest -L gui` (both).

The ImGui-free view-model suites (`tests/unit/gui_*_test.cpp` in `smbios_unit_test`) stay where they are and are not relabelled.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [Tier scaffolding: option, definitions, labels](01-tier-scaffolding.md) | ⬜ Not started | 02, 04 |
| 02 | [Headless feasibility spike and `gui-ci` CI wiring](02-headless-spike-and-ci-wiring.md) | ⬜ Not started | 03 |
| 03 | [CI-safe tier: headless widget tests](03-ci-safe-headless-widget-tests.md) | ⬜ Not started | 05 |
| 04 | [Local-only tier and test-only auto-exit build](04-local-only-tier-and-auto-exit.md) | ⬜ Not started | 05 |
| 05 | [Tests, docs and exit gate](05-tests-docs-and-exit-gate.md) | ⬜ Not started | - |

The story has five tasks rather than four because the feasibility spike needs real CI runners on three OSes, which means the `ctest -L gui-ci` CI step has to exist first. So the minimal CI wiring moves forward into Task 02, next to the spike. Task 05 keeps the doc updates and the acceptance gate.

## Key constraints

- **Three gates, all enforced.** `tests/gui/` is added only when `SMBIOS_BUILD_GUI=ON`. `tests/gui/local/` is added only when `SMBIOS_GUI_LOCAL_TESTS=ON`. Every local-only source fails to compile unless `SMBIOS_GUI_LOCAL_ONLY_TESTS` is defined. `SMBIOS_GUI_LOCAL_TESTS` defaults to ON for a GUI build outside CI, and it is **forced OFF** with a message when the environment variable `GITHUB_ACTIONS` or `CI` is set at configure time. Task 01 has the details.
- **The CI tier never skips, and the local tier never fails for a missing resource.** A `gui-ci` case that cannot run is a failure. A `gui-local` case that lacks a display, GL context, clipboard or readable table prints exactly one line, `SKIP: <case>: <reason>`, and CTest reports it as *Skipped* rather than *Failed*.
- **The shipped `smbios_gui` binary stays unchanged.** The CI tier compiles `entry_tree_view.cpp`/`copy_as_action.cpp` straight into its own test executable, following the precedent of `tests/unit/` compiling `csv_formatter.cpp`. The `smbios_gui` target's sources, flags and link line stay as they are. The auto-exit hook (Task 04) is the only source change to `smbios_gui/src/main.cpp`. It sits behind a compile definition that only a separate, test-only twin target sets, so the shipped binary does not contain it. Task 04 lists the alternatives and states that this is a maintainer decision.
- **The security stance of [docs/security/2026-11-11-gui-browsing-ui.md](/docs/security/2026-11-11-gui-browsing-ui.md) still holds.**
  - The GUI stays read-only, with no editable widgets.
  - Firmware strings reach ImGui only through `display_text()` + `TextUnformatted`.
  - The tests verify these rules. They must not loosen any of them, and they must not add a test seam to production rendering code.
  - The auto-exit hook gets its own threat note, written after a security-auditor check before implementation (Task 04).
- **No serials, UUIDs or asset tags in any test output.**
  - The CI tier uses only synthetic fixtures (threat model requirement 20).
  - Local-tier output contains counts, structure types and node indices only.
  - CLI output captured by a local test is held in memory, never written to a file, and never echoed.
- **Report discrepancies only.** Parity-style checks print nothing on success apart from the one-line CTest result. On failure they print one line per difference, in the form `<what>: expected -> actual`.
- **The ratified build and CI shape stays as it is.** The GUI stays opt-in (`SMBIOS_BUILD_GUI`, vcpkg feature `gui`, `<os>-gui` presets). The CI change is limited to the existing `gui-build` job and keeps its bounded-output approach: full output goes to a file, failure prints the first diagnostic lines plus a short tail, and success prints one summary line. No new job, no presets change, and no `ctest -R` regex change in the `build`/`sanitizers` jobs. A separate CI-rework milestone will replace `ci.yml` later.
- **No new third-party dependency.** Only the pinned `imgui`/`glfw3` and Boost.Test are used. The tests may use `imgui_internal.h`. That internal-API coupling is accepted for test code only, because the vcpkg baseline is pinned, and a test-support header documents it.

## Non-goals

- Pixel, screenshot or visual-regression assertions of any kind (unchanged from `plan.md`).
- Running `smbios_gui` (or its test twin) on CI, `xvfb-run`, or a software-GL renderer on runners.
- Dear ImGui Test Engine. It ships under its own license, not MIT, which conflicts with ADR 0003 DD3, and it would add a dependency.
- Sanitizer builds of the GUI tiers in CI. They are allowed locally. Adding them to CI is left to the CI-rework milestone.
- Automating manual-testing.md's checks that need human judgement: visual layout, M-12 (running as root), M-13 (resize/minimise/multi-monitor), and the Escape-key and title-bar paths of M-01.

## Open questions

- **Does a headless ImGui 1.92.8 frame work on all three hosted runners?** Task 02's spike answers this. Any part that turns out not to be feasible moves to the local tier or stays manual. Task 02 says which.
- **Is a widget locator available without pixel constants?** `ImGui::ActivateItemByID()` + `ImGuiWindow::GetID()` from `imgui_internal.h` is the candidate. If it does not work, the input-driven cases (hex toggle, combo, Copy click, node click) stay manual and the CI tier keeps only the input-free cases.
- **Can macOS runners create hidden GLFW windows?** This decides nothing here, because the local tier never runs on CI. It is recorded only so nobody reopens it.
- **Which auto-exit hook form should Task 04 use?** It is a maintainer decision. Task 04 recommends a compile-time test-only twin and lists the runtime-flag alternative.

## Files or directories created or modified by this story

- Root `CMakeLists.txt` (option `SMBIOS_GUI_LOCAL_TESTS` + CI detection), `tests/CMakeLists.txt` (conditional `add_subdirectory(gui)`).
- `tests/gui/CMakeLists.txt`, `tests/gui/support/`, `tests/gui/ci/`, `tests/gui/local/` (new).
- `smbios_gui/CMakeLists.txt` (test-only twin target, `SMBIOS_GUI_LOCAL_TESTS=ON` only), `smbios_gui/src/main.cpp` (hook behind `SMBIOS_GUI_TEST_HOOKS`; recommended form, pending the maintainer decision).
- `.github/workflows/ci.yml` (`gui-build` job only).
- `docs/security/` (auto-exit threat note), `docs/test/manual-testing.md`, `docs/test/test-subsystem.md`, `docs/dev/cpp_coding_standard.md` (Testing table), root `CLAUDE.md`, this milestone's `status.md`.
