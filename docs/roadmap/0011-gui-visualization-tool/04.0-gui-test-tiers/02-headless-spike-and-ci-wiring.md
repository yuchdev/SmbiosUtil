# 02 - Headless feasibility spike and `gui-ci` CI wiring

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Prove, on real GitHub-hosted runners for all three OSes, that the vendored Dear ImGui (1.92.8, pinned baseline) can run complete frames with no window, no GL backend and no OS clipboard, and that each of the CI-tier oracles works. Then wire `ctest -L gui-ci` into the existing `gui-build` job. The spike result decides what Task 03 automates and what moves to the local tier or stays manual.

## Spike items (each recorded PASS / FAIL / PARTIAL per OS in `status.md`)

| # | Question | Candidate approach (verify; do not assume) |
|---|----------|---------------------------------------------|
| S1 | Headless frame loop | `CreateContext()`, `IniFilename = LogFilename = nullptr`, `io.DisplaySize` fixed (for example 1280x720), `io.DeltaTime = 1/60`. Font atlas: either set `io.BackendFlags \|= ImGuiBackendFlags_RendererHasTextures` (what upstream `examples/example_null` does in 1.92) and after each `Render()` mark every `ImDrawData::Textures` entry whose status is not OK as OK with a dummy non-zero tex ID, or use the legacy `io.Fonts->Build()` path. Pick the one that runs **at least 1000 frames** with no assert and no unbounded texture-request growth. |
| S2 | No OS side effects | Override every `ImGuiPlatformIO` OS hook: `Platform_SetClipboardTextFn`/`GetClipboardTextFn` with an in-memory capture, `Platform_OpenInShellFn`/`Platform_SetImeDataFn` with no-ops or null. Windows matters most here, because the default clipboard handler uses the Win32 clipboard. |
| S3 | Balance oracle | `ImGui::ErrorRecoveryStoreState()` snapshots taken before and after a call to `render_entry_browser()` inside a test-owned `Begin`/`End` compare equal (`imgui_internal.h`). Find out whether `IM_ASSERT` is live in the linked imgui build, which depends on whether vcpkg selects the release or debug variant for the preset's build type. The oracle must not depend on it. |
| S4 | Text oracle | `ImGui::LogToBuffer()` / `LogFinish()` and `GImGui->LogBuffer` capture the text that `TextUnformatted`, `Selectable` and table cells draw for visible items. Confirm that clipped-away rows are absent. |
| S5 | Widget locator | Find the child window by name fragment (`##entry_list`, `##entry_fields`) by iterating `GImGui->Windows`, compute the item ID with that window's `GetID()` (and the index scope for `PushID(index)` + `"##node"`), call `ImGui::ActivateItemByID(id)`, then render one frame. Confirm that the Hex checkbox toggles, the Copy button fires the captured clipboard, and the `##format` combo opens. |
| S6 | ID-conflict signal | Check whether 1.92.8 exposes a per-frame duplicate-ID signal usable from tests (`io.ConfigDebugHighlightIdConflicts` and the context state behind it). Record the name, or record "none". |
| S7 | Process start on runners | `smbios_gui_ci_test` starts with no `DISPLAY` on `ubuntu-latest` (the X11 libraries are linked through `imgui::imgui` -> `glfw` but never called), and on `windows-latest` with no missing-DLL dialog. Recall the `smbios_c.dll` hang recorded in `ci.yml`. The `--timeout 60` turns a hang into a fast failure. |

**Fallback rule.** A FAIL in S1, S2 or S7 on any OS stops the story at this task and goes to the maintainer, because the CI tier cannot exist on that OS. A FAIL in S3, S4 or S6 drops only the cases that rely on that oracle. A FAIL in S5 moves the input-driven cases (hex toggle, combo, Copy click, node click) to "remains manual". They do not go to the local tier, which would need the same locator. Record each outcome as `expected -> actual`.

## Files

- **Create** `tests/gui/support/headless_imgui.hpp`/`.cpp`, CI-tier test support with Doxygen on every declaration:
  - `class HeadlessImGuiContext`: RAII owner of one ImGui context, configured as S1/S2 found works. It is non-copyable. Its destructor calls `DestroyContext()`.
  - `void HeadlessImGuiContext::run_frame(const std::function<void()>& body)`: does `NewFrame()`, opens a fixed full-viewport test window (literal ID), runs `body`, then `End()`, `Render()` and texture-request handling.
  - `std::string HeadlessImGuiContext::run_frame_capturing_text(const std::function<void()>& body)`: the same, wrapped in `LogToBuffer()`/`LogFinish()`. Returns the captured text. Test code only; never printed (see Constraints).
  - `const std::string& HeadlessImGuiContext::clipboard() const`: the bytes last given to the clipboard hook.
  - `bool HeadlessImGuiContext::activate_item(std::string_view child_window_fragment, ...)`: the S5 locator. Present only if S5 passes.
  - A file comment naming every `imgui_internal.h` symbol used, and stating that a vcpkg baseline bump must re-run this suite.
- **Create** `tests/gui/ci/headless_spike_test.cpp` (`BOOST_AUTO_TEST_SUITE(GuiHeadlessSpikeTests)`): one case per spike item S1-S6 that passes on all runners, against a small `SyntheticTableBuilder` table. Spike code that turns out non-viable is deleted, not left disabled.
- **Modify** `.github/workflows/ci.yml`, **`gui-build` job only**, inside the existing `Configure and build` step's script (no new step or job, so the change stays minimal):
  - After the `ok: <binary>` line, add `run_quiet test ctest --test-dir build/${{ matrix.preset }} -C Debug -L gui-ci --timeout 60 --output-on-failure`. `-C Debug` is required for the multi-config Windows generator and harmless with Ninja. On success, print one line: `ok: ctest -L gui-ci (<N> tests passed)`.
  - Add the guard that the local tier is absent: `ctest --test-dir build/${{ matrix.preset }} -C Debug -N -L gui-local` must report `Total Tests: 0`, and no `smbios_gui_local_test*` file may exist under the build tree. Otherwise print `::error::local-only GUI tests are configured on CI: expected 0 -> <N>` and exit 1. On success, print `ok: no gui-local tests configured`.
  - Update the job's top comment: CI still never runs `smbios_gui` itself, and now runs the headless `gui-ci` tier. Reference this story.
  - The existing `grep -E` diagnostic pattern already matches Boost.Test's `error: in "..."` and CTest's `The following tests FAILED:`. Confirm that against a deliberately failing run (Tests). Do not widen it.

## Functions and Classes

- `smbios::test::gui::HeadlessImGuiContext`, test-only, as above.

## Constraints

- **No GLFW or backend in this tier.** No `glfw*` call and no `imgui_impl_*.h` include.
- **Synthetic data only.** `SMBios` is constructed, then replaced through `load_table()` (`gui_test_support.hpp`) **before** any render, so a runner's live table never reaches a frame.
- **Captured text and clipboard bytes are never printed.** Failure messages name the case, the node or row index, and expected or actual *counts*. The only exception is a synthetic literal that the test itself defined.
- **Neither the `build` nor the `sanitizers` job is edited, and no existing `ctest -R` regex changes.**

## Tests

- Push the branch, then run the `gui-build` job on ubuntu, windows and macos. Each job shows `ok: ctest -L gui-ci (...)` and `ok: no gui-local tests configured`.
- In one throwaway push (reverted), make one spike case fail deliberately. The job fails on all three OSes, and the output shows the `::error::` line, at most 20 diagnostic lines and the 40-line tail, with no captured text.

## Success criteria

- [ ] S1-S7 recorded per OS in `status.md`, with the chosen approach for S1, the existence or absence of the S6 signal, and the fallback applied to each FAIL.
- [ ] `gui-build` runs `ctest -L gui-ci` green on all three OSes, with output bounded as above.
- [ ] CI proves that `gui-local` has 0 tests and no binary.
- [ ] Task 03's case list is amended to match the spike outcome before Task 03 starts.
