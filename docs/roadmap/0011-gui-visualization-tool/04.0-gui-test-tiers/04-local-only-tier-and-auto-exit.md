# 04 - Local-only tier and test-only auto-exit build

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started. **Blocked on:** (a) the maintainer's choice of hook form (see "Auto-exit hook" below); (b) a pre-implementation check by `security-auditor` of the chosen hook.

> **Ruled 2026-11-11 (maintainer):** the auto-exit hook takes the **test-copy define** form (`SMBIOS_GUI_TEST_HOOKS`, only the `smbios_gui_autoexit` test copy, built locally only; shipped binary unchanged) - not a command-line flag and not an environment variable. The clipboard round-trip test **saves and restores clipboard text** and is part of the normal `gui-local` tier (non-text clipboard content is lost; documented), not opt-in. Open questions in this spec on these two points are closed.

## Objective

Automate the manual-testing.md checks that need a real display, a GL context, the OS clipboard or the live table. These run only on a developer machine (`ctest -L gui-local`), and each case skips cleanly when its resource is missing.

## Cases (`tests/gui/local/`, suite `SmbiosGuiLocalTests`, one CTest entry each)

| Case | Needs | What it does / asserts | Skip when |
|------|-------|------------------------|-----------|
| `HiddenWindowSmoke` | display, GL | `glfwInit`, then a hidden window (`GLFW_VISIBLE` false, same context hints as `main.cpp`), the GLFW + OpenGL3 backends, and the live `SMBios` + `build_entry_tree()`. It runs 300 frames of `render_entry_browser()` with `glfwSwapInterval(0)` (a hidden window with vsync can stall on macOS), then tears down in reverse order. Asserts: no crash, balanced stacks every frame (Task 02 S3), and every teardown call reached. | `glfwInit` or `glfwCreateWindow` fails, or `ImGui_ImplOpenGL3_Init` fails |
| `ClipboardRoundTripMatchesCli` | display (clipboard), live table, `smbios` CLI | For every registered node and each of `json`, `text`, `xml` and `yaml`: `render_entry_as()` -> `glfwSetClipboardString` -> `glfwGetClipboardString`. The bytes must be equal, and they must occur verbatim in the in-memory stdout of `smbios --format <name>` (run through `popen`/`_popen`; the CLI adds document framing, so test containment). Before starting, save the user's prior clipboard text; restore it afterwards, including on failure (RAII). Print one line noting that the clipboard was used and restored. Covers M-09 and M-10. | no display, or 0 structures |
| `LiveParityRegisteredNodesMatchCliText` | live table, `smbios` CLI | The registered nodes of `build_entry_tree()` (registered means `SMBiosEntryFactory::create()` is non-null, the same predicate the CLI uses to skip) must match the `Header type: N` lines of `smbios --format text` in count and in type order. Report discrepancies only, for example `registered node count: 26 -> 25` or `node #7 type: 17 -> 19`. Also print one informational line on success: `GUI-only unregistered nodes: <count>`. No display needed. Covers M-02 and M-03. | 0 structures readable (for example, an unprivileged Linux run). The skip line says `no readable SMBIOS table`, and never suggests `sudo`. |
| `LaunchAutoExitTwin` | display, GL | Spawn the test twin (see below) with `--exit-after-frames 120`, from a fresh empty temporary working directory. Asserts: exit status 0, combined stdout+stderr is 0 bytes, and the working directory is still empty afterwards (no `imgui.ini`). Covers M-01 (launch and clean exit) and M-14. On failure, report sizes and status only, for example `exit status: 0 -> 1` or `stdout+stderr bytes: 0 -> 57`. | no display, checked with the same probe as `HiddenWindowSmoke` before spawning |

- Paths come from compile definitions: `SMBIOS_CLI_PATH="$<TARGET_FILE:smbios>"` and `SMBIOS_GUI_AUTOEXIT_PATH="$<TARGET_FILE:smbios_gui_autoexit>"`, plus `add_dependencies` on both.
- The display probe is in `support/glfw_test_window.{hpp,cpp}` (Doxygen). It is an RAII hidden window + context + backends, used by `HiddenWindowSmoke`, `ClipboardRoundTripMatchesCli` and the probe for `LaunchAutoExitTwin`.

## Auto-exit hook (maintainer decision; recommended option A)

| Option | Shipped `smbios_gui` | Tests the real artifact? | Attack surface |
|--------|---------------------|-------------------------|----------------|
| **A (recommended) - compile-time test twin** | Byte-for-byte unaffected. `main.cpp` gains a block under `#if defined(SMBIOS_GUI_TEST_HOOKS)` that the shipped target never defines. | No. It tests a twin built from the same sources and link line with one extra define. That catches startup, link and teardown regressions, but not artifact-specific ones. | None in the shipped binary |
| B - runtime flag `--exit-after-frames N` in the shipped binary | Gains argv parsing (today it is `int main()`) | Yes | Small: a strictly parsed flag that can only make the program exit early, but it is new argv parsing in a shipped GUI binary |
| C - environment variable `SMBIOS_GUI_EXIT_AFTER_FRAMES` | Gains an environment read | Yes | Small but ambient: inherited by every child, invisible on the command line. Not recommended. |

Specification for option A (if B is chosen instead, the parse rules below apply unchanged to the shipped binary, and the twin target is dropped):

- **`smbios_gui/CMakeLists.txt`:** inside `if(SMBIOS_GUI_LOCAL_TESTS)`, after `smbios_gui`, add the target `smbios_gui_autoexit`, built from the same `${SOURCES}`, with the identical include, compile, link and whole-archive settings, plus `target_compile_definitions(PRIVATE SMBIOS_GUI_TEST_HOOKS=1)`. Its output directory is under the test bin directory, not `smbios_gui/bin/`, so nobody mistakes it for the app. It has no install rule (none exist for `smbios_gui` either). Because `SMBIOS_GUI_LOCAL_TESTS` is forced OFF there, CI never builds it.
- **`smbios_gui/src/main.cpp`** (only under the define):
  - `main(int argc, char** argv)` accepts exactly `--exit-after-frames N`. `N` is a decimal integer from 1 to 100000, digits only, with no sign, whitespace or suffix.
  - Any other argv makes the program print the fixed line `smbios_gui: usage: --exit-after-frames <1-100000>` to stderr and exit 2. It never echoes argv.
  - After N **rendered** frames, it calls `glfwSetWindowShouldClose()` and leaves through the normal teardown path, so the exit status is 0.
  - Without the define, `int main()` and all behaviour are unchanged.
  - Update the file-level Doxygen.
- **Proof that the hook is inert:** the string `--exit-after-frames` occurs 0 times in the built `smbios_gui` binary and at least once in `smbios_gui_autoexit` (checked with `grep -c` on the binaries, recorded in `status.md`).
- **Threat note:** before implementation, ask `security-auditor` to review the chosen option against [docs/security/2026-11-11-gui-browsing-ui.md](/docs/security/2026-11-11-gui-browsing-ui.md). Record the result as a dated addendum in that file, or as a new dated `docs/security/` file if the auditor prefers. The review must cover argv parsing, no echo of input, exit-only effect, the twin never being built on CI or installed, and (for option B) how the flag is documented, given that the GUI has no `--help`.

## manual-testing.md coverage after this story

Task 05 applies this mapping to `docs/test/manual-testing.md`, adjusted for Task 02/03 outcomes. A check is marked automated only if its case actually passed.

| Check | Becomes | By |
|-------|---------|-----|
| M-01 launch and exit | Partly automated: launch and exit 0 | `LaunchAutoExitTwin`. Escape and title-bar close stay manual. |
| M-02 node list vs CLI | Automated | `LiveParityRegisteredNodesMatchCliText` |
| M-03 unregistered types | Automated | CI `RegisteredAndUnknownNodesRender` + local parity's extras line |
| M-04 field table vs CLI | Automated | CI `FieldRowsMatchFieldValueText` + local `ClipboardRoundTripMatchesCli` (json) |
| M-05 value kinds | Automated (rendering); look and feel manual | CI `FieldRowsMatchFieldValueText`, `HexToggle...` |
| M-06 hex toggle | Automated if S5 passed | CI `HexToggleSwitchesNumericRendering` |
| M-07 selection and scrolling | Partly automated | CI clamp, click and clipper cases. Flicker and feel stay manual. |
| M-08 Copy dropdown | Automated if S5 passed | CI `GuiCopyAsHeadlessTests` |
| M-09 Copy json vs CLI | Automated | CI click path + local OS-clipboard round trip |
| M-10 Copy other formats | Automated | local `ClipboardRoundTripMatchesCli` |
| M-11 no readable table | Partly automated | CI `EmptyNodesShowsNoTableText`. The startup exit-1 path stays manual. |
| M-12 root warning | Manual | Never automated: tests must never run as root |
| M-13 window behaviour | Manual | |
| M-14 stray output and files | Automated (output and files) | `LaunchAutoExitTwin`. "No text inputs" is already enforced by review and grep (G-11). |

## Constraints

- **Never on CI.** Every source starts with `local_only_guard.hpp` (Task 01).
- **Privacy.** CLI stdout and rendered entries are held only in memory. They are never written to a file, never printed, and never included in a failure message. The clipboard holds live data only for the duration of the round trip and is then restored.
- **No privilege escalation.** No case uses `sudo` or advises it. A Linux run without a readable table skips.
- **The shipped executable's behaviour is unchanged** under option A. The only production source change is the define-gated hook block.

## Tests

The cases above, run on macOS (the maintainer's host) with `ctest --test-dir build/macos-gui -L gui-local --output-on-failure`. Also, one run from a shell with no display (for example over `ssh` with no `DISPLAY` on Linux, if available) to show that the display-dependent cases report *Skipped*, not *Failed*.

## Success criteria

- [ ] The maintainer's hook decision and the security-auditor note are recorded before any `main.cpp` edit.
- [ ] All four cases pass on macOS. `LiveParity...` reports no discrepancy, and on the maintainer's Mac it reproduces the ruled 26 = 26 with the extras line showing 6.
- [ ] Skips are one line each, and CTest shows them as *Skipped*.
- [ ] The inertness proof (string count 0 vs >= 1) is recorded.
- [ ] No serial, UUID or asset tag appears in any test output or `status.md` entry.
