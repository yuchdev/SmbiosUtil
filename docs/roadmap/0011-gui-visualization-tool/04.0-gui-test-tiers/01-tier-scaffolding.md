# 01 - Tier scaffolding: option, definitions, labels

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-11; PASS; macOS-verified, CI proof of the 'local tier absent' guard is Task 02/05)

## Objective

Create the build-graph skeleton for the two GUI test tiers so that four things hold:

- A GUI build outside CI configures, builds and registers both tiers.
- A GUI build under CI configures and builds only the CI-safe tier. The local-only tier is not compiled at all.
- A non-GUI build adds nothing.
- Each tier can be run on its own with `ctest -L`.

The test bodies in this task are placeholders. Tasks 02-04 fill them in.

## Files

- **Modify** root `CMakeLists.txt`, next to `option(SMBIOS_BUILD_GUI ...)`:
  - Detect CI at configure time: `SMBIOS_CI_ENVIRONMENT` is ON if `DEFINED ENV{GITHUB_ACTIONS}` or `DEFINED ENV{CI}`.
  - `option(SMBIOS_GUI_LOCAL_TESTS "Build/register the local-only GUI tests (display/GL/clipboard/live table; never on CI)" <default>)`. The default is `ON` iff `SMBIOS_BUILD_GUI AND NOT SMBIOS_CI_ENVIRONMENT`. Compute it before the `option()` call, so it only seeds a fresh cache.
  - If `SMBIOS_CI_ENVIRONMENT` is ON and `SMBIOS_GUI_LOCAL_TESTS` is ON, force it OFF with `set(... OFF CACHE BOOL "" FORCE)` and print one line: `message(WARNING "SMBIOS_GUI_LOCAL_TESTS forced OFF: CI environment detected (GITHUB_ACTIONS/CI set); the local-only GUI tier never runs on CI.")`. There is no override.
  - If `SMBIOS_GUI_LOCAL_TESTS` is ON and `SMBIOS_BUILD_GUI` is OFF, the option is ignored and a `STATUS` line says so. This is deliberately not `FATAL_ERROR`, because a cached ON from an earlier GUI configure must not break a later non-GUI reconfigure of the same tree.
  - Add a comment block in the style of the existing `SMBIOS_BUILD_FUZZERS`/`SMBIOS_BUILD_GUI` comments.
- **Modify** `tests/CMakeLists.txt`: add `if(SMBIOS_BUILD_GUI) add_subdirectory(gui) endif()`.
- **Create** `tests/gui/CMakeLists.txt`:
  - **`smbios_gui_ci_test`** (always, inside this directory):
    - Sources: `ci/*.cpp`, `support/headless_imgui.{hpp,cpp}`, and `${CMAKE_SOURCE_DIR}/smbios_gui/src/entry_tree_view.cpp` + `copy_as_action.cpp`. Those two files are compiled in directly, following the `csv_formatter.cpp` precedent in `tests/unit/CMakeLists.txt`. Do not edit the `smbios_gui` target. Also compile in `tests/unit/synthetic_table_builder.cpp` (plus `doc_sample_fixture.cpp` if Task 03 uses it), with `tests/unit/` on the include path for `gui_test_support.hpp`.
    - Link `Boost unit_test_framework`, `libsmbios_gui_model`, `libsmbios` (the identical three-way whole-archive block), and `imgui::imgui`. Do not link `OpenGL::GL`, and make no direct `glfw` call. `imgui::imgui` pulls in `glfw` transitively through the vcpkg port. That is accepted, because nothing in this tier calls into it.
    - `${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS}` (`-Werror`/`/WX`, same as `smbios_gui`), `${SMBIOS_LINK_OPTIONS}`. Output directory is the same as the other test executables.
    - `add_test(NAME smbios_gui_ci_test COMMAND smbios_gui_ci_test)` with `set_tests_properties(... PROPERTIES LABELS "gui;gui-ci" TIMEOUT 60)`.
  - **`smbios_gui_local_test`** (only `if(SMBIOS_GUI_LOCAL_TESTS)`):
    - Sources: `local/*.cpp` + `support/glfw_test_window.{hpp,cpp}`.
    - `target_compile_definitions(PRIVATE SMBIOS_GUI_LOCAL_ONLY_TESTS=1)`.
    - Links everything the CI tier links plus `glfw` and `OpenGL::GL`. `GL_SILENCE_DEPRECATION` on Apple, as in `smbios_gui`.
    - Registration: **one CTest entry per test case**, named `smbios_gui_local_test.<CaseName>` with `--run_test=SmbiosGuiLocalTests/<CaseName>`, each with `LABELS "gui;gui-local"`, `TIMEOUT 120`, and (`if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)`) `SKIP_REGULAR_EXPRESSION "^SKIP: "`. Per-case entries are what let a skipped case show as *Skipped* in CTest without hiding its passing siblings.
    - Keep the case list in one CMake list variable next to the sources.
  - Use test names that cannot collide with any existing `ctest -R` filter in `ci.yml`, such as `smbios_c_`, `smbios_unit_test` or `smbios_example_`. `smbios_gui_*` does not match any of them. Check it.
- **Create** `tests/gui/support/local_only_guard.hpp`: `#if !defined(SMBIOS_GUI_LOCAL_ONLY_TESTS)` / `#error "local-only GUI test compiled without SMBIOS_GUI_LOCAL_ONLY_TESTS; see tests/gui/CMakeLists.txt"`. Every `tests/gui/local/*.cpp` and `support/glfw_test_window.*` includes it first.
- **Create** placeholder sources so both tiers build and register:
  - `tests/gui/ci/gui_ci_main.cpp`, holding `BOOST_TEST_MODULE SmbiosGuiCiTests` and one trivial case.
  - `tests/gui/local/gui_local_main.cpp`, holding `BOOST_TEST_MODULE SmbiosGuiLocalTests`, and a skip helper (below) with one case that exercises it.
- **Create** the skip helper `tests/gui/support/local_skip.hpp` (local-only, guarded). It is a function returning `boost::test_tools::assertion_result` for use with `*boost::unit_test::precondition(...)`. When the resource is missing, it prints exactly one line to stdout, `SKIP: <CaseName>: <reason>` (for example `SKIP: HiddenWindowSmoke: no display (glfwInit failed)`), and returns false.

## Functions and Classes

- `smbios::test::gui::skip_unless(bool available, std::string_view case_name, std::string_view reason) -> boost::test_tools::assertion_result`: local-only, Doxygen-commented.
- No production function or class changes.

## Constraints

- **Local-only sources are never compiled on CI.** CMake (`SMBIOS_GUI_LOCAL_TESTS`) enforces this, and the definition guard backs it up. Neither layer alone is the proof. Task 05 proves it on CI.
- **The `ci/` directory must never include `local_only_guard.hpp`, GLFW headers or `imgui_impl_*.h`.** Task 05's grep enforces this.
- **A default (`<os>`, non-GUI) configure gains no target and no test.** `ctest -N` on `build/<os>` is unchanged.
- `file(GLOB)` is used as elsewhere. Re-run `cmake` after adding sources (root `CLAUDE.md`).
- Doxygen on every test-support declaration, per `docs/dev/cpp_coding_standard.md`.

## Tests

- **Local, macOS `macos-gui`:** `ctest -N -L gui-ci` lists 1 test, and `ctest -N -L gui-local` lists at least 1.
- **Local:** `CI=1 cmake --preset macos-gui -B build/ci-sim` prints the forced-OFF warning only when `-DSMBIOS_GUI_LOCAL_TESTS=ON` is also passed. In both cases, `ctest -N -L gui-local --test-dir build/ci-sim` shows `Total Tests: 0`, and no `smbios_gui_local_test` binary exists under `build/ci-sim`.
- **Local:** temporarily compiling one `local/*.cpp` without the definition (scratch edit, reverted) fails with the `#error` text.
- **Local:** `cmake --preset macos` (no GUI) leaves `ctest -N` unchanged from before this task.

## Success criteria

- [ ] Both tiers configure, build (`-Werror`) and pass their placeholder case on the `macos-gui` preset. `ctest -L gui` runs both.
- [ ] Under `CI=1` or `GITHUB_ACTIONS=true`, the local tier does not exist: there is no target, no binary and no CTest entry.
- [ ] A skipped local case is reported by CTest as *Skipped*, with one `SKIP:` line.
- [ ] The `smbios_gui` target is unchanged: its sources, definitions and link line show no diff.

## As-built notes (2026-11-11)

- **Delivered:** `SMBIOS_CI_ENVIRONMENT` (plain variable, recomputed every configure, cannot be overridden with `-D`), `option(SMBIOS_GUI_LOCAL_TESTS)` seeded ON only for a non-CI GUI build and forced OFF with the one-line warning under `GITHUB_ACTIONS`/`CI`, `tests/gui/` (`smbios_gui_ci_test` labelled `gui;gui-ci`; `smbios_gui_local_test` registered as one CTest entry per case, labelled `gui;gui-local`), `local_only_guard.hpp` (`#error` without `SMBIOS_GUI_LOCAL_ONLY_TESTS`, set only as a PRIVATE definition on the local target), `local_skip.hpp` (`skip_unless()`), placeholder cases, and registry rows in `docs/test/test-subsystem.md`. `smbios_gui/`, `CMakePresets.json` and `.github/` are untouched.
- **Fail closed (threat-model req 2):** `tests/gui/CMakeLists.txt` returns before creating any local target unless `SMBIOS_BUILD_GUI AND SMBIOS_GUI_LOCAL_TESTS AND NOT SMBIOS_CI_ENVIRONMENT`, independent of the cache forcing; proven with a stale-cache reconfigure under `GITHUB_ACTIONS=true` (local tests/targets/binaries all 0). Task 04's `smbios_gui_autoexit` must go behind the same condition.
- **Spec correction - skip regex:** the spec's `SKIP_REGULAR_EXPRESSION "^SKIP: "` does NOT work: CMake anchors `^` to the start of the whole output, which begins with Boost.Test's `Running 1 test case...` line, so the always-skipping case was reported Passed. Use `"(^|\n)SKIP: "` (CMake turns `\n` into a real newline; also matches Windows `\r\n` output); verified to report Skipped on macOS.
- **Deviations:** two local placeholder cases (a passing sibling proves a skip is per-case); `support/headless_imgui.*`, `support/glfw_test_window.*` and `doc_sample_fixture.cpp` are added by Tasks 02-04 as needed.
- **Known stale-tree caveat:** a build tree configured with the local tier ON and later reconfigured under CI keeps an empty `CMakeFiles/smbios_gui_local_test.dir/` directory; Task 02's fail-closed `find` (no `-type f`) would flag it, which is correct. Real CI always configures a fresh tree.
- **Not run:** clang-tidy and sanitizer builds (build wiring and placeholders only). `build/macos` in this working tree still has `SMBIOS_BUILD_GUI=ON` cached from earlier work, so it reports GUI tests; a fresh default configure reports the original 21.

