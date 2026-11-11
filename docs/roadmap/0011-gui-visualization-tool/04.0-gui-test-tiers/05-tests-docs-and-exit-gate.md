# 05 - Tests, docs and exit gate

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Close Story 04.0. The docs must say how the two tiers are run and what remains manual, and the maintainer's acceptance bar must be met and recorded: "build and all tests pass locally and on CI".

## Files

- **Modify** root `CLAUDE.md`, Build and test section: one bullet for the GUI tiers. It names `tests/gui/ci/` (`gui-ci`, headless, runs in `gui-build` CI) and `tests/gui/local/` (`gui-local`, display/clipboard/live table, never on CI). It also names `SMBIOS_GUI_LOCAL_TESTS` (default ON for GUI builds outside CI, forced OFF under `CI`/`GITHUB_ACTIONS`), `SMBIOS_GUI_LOCAL_ONLY_TESTS`, `ctest -L gui-ci|gui-local|gui`, and, under option A, the `smbios_gui_autoexit` twin. Correct any GUI bullet that still says CI never runs anything GUI.
- **Modify** `docs/test/test-subsystem.md`:
  - Add a taxonomy row for each tier: GUI CI-safe as Unit-classified and always green; GUI local-only as Integration-classified and skips without resources.
  - Replace the "GUI ... cannot be driven headlessly" paragraph.
  - Add an implemented-coverage subsection that lists the suites at group level.
- **Modify** `docs/dev/cpp_coding_standard.md`, Testing table: add the `tests/gui/ci/` and `tests/gui/local/` rows, and the rule that local-only sources must include `local_only_guard.hpp` and may only skip, never fail, for a missing resource.
- **Modify** `docs/test/manual-testing.md`:
  - Apply Task 04's coverage table, as finally achieved, to the "What is already automated" paragraph and to the platform matrix (mark automated checks as "auto (`<case>`)").
  - Keep full manual steps only for what remains manual.
  - Add a "Run the automated GUI tiers first" line: `ctest --test-dir build/<os>-gui -L gui`.
- **Modify** this milestone's `status.md`: set the Story 04.0 row and add a Story 04.0 section with Delivered, Tests / gate, and the reconciliation note.

## Tests (the acceptance gate)

1. **Local, macOS `macos-gui`, fresh build directory:** configure, then build with `-Werror`, then run `ctest --test-dir build/macos-gui --output-on-failure`. All tests pass: the default suites plus `gui-ci` plus `gui-local`. Any skip is listed with its one-line reason. Record the counts.
2. **Local, default `macos` preset:** the full `ctest` result is unchanged from before this story, with no GUI target or test.
3. **CI, real push:** the `build`, `sanitizers` and `gui-build` jobs are green on all three OSes. Each `gui-build` log shows `ok: <smbios_gui binary>`, `ok: ctest -L gui-ci (<N> tests passed)` and `ok: no gui-local tests configured`.
4. **Proof that no local-only test runs on CI.** All three must hold and be recorded:
   - The CI guard line from Task 02.
   - The configure log on CI does not contain `SMBIOS_GUI_LOCAL_TESTS forced OFF` unless the option was passed. With a fresh cache it simply defaults OFF.
   - No `smbios_gui_local_test*` or `smbios_gui_autoexit*` file exists in the build tree. The Task 02 guard checks this.
5. **Static checks:** a grep shows that `tests/gui/ci/` includes no `glfw`, `imgui_impl_` or `local_only_guard`, and that every `tests/gui/local/*.cpp` includes `local_only_guard.hpp` first. `python scripts/check_doc_links.py docs/` is clean.

Report results as discrepancies only (`expected -> actual`). A clean gate is one line per environment.

## Success criteria

- [ ] Gate items 1-5 hold and are recorded in `status.md` with counts only.
- [ ] `manual-testing.md` names exactly the checks that remain manual (at minimum M-12 and M-13, plus whatever the spike left manual).
- [ ] `CLAUDE.md`, `test-subsystem.md` and `cpp_coding_standard.md` describe both tiers and how to run them.
- [ ] Milestone 0011's `status.md` Story 04.0 row is set to ✅.
