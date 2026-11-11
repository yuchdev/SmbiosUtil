# Threat Model - smbios_gui test tiers (milestone 0011, Story 04.0) - 2026-11-11

Pre-implementation review. Scope: `docs/roadmap/0011-gui-visualization-tool/04.0-gui-test-tiers/`
(README + Tasks 01-05, including the "Ruled 2026-11-11" note in Task 04), checked against
`smbios_gui/{src,include}`, `smbios_gui/CMakeLists.txt`, `tests/unit/CMakeLists.txt`, the root
`CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml` and the pinned Dear ImGui
1.92.8 sources (vcpkg buildtree). This is the "auto-exit hook threat note" Task 04 asks for.

**Standing input.** Every coder briefing for Story 04.0 must include the "Per-task requirements"
section below. Every requirement in
[2026-11-11-gui-browsing-ui.md](2026-11-11-gui-browsing-ui.md) still holds, in particular G-1
(firmware text only via `display_text()` + `TextUnformatted`), G-8 (never advise root), G-11
(no `imgui.ini`, no editable widgets) and requirement 20 (no real identifiers in fixtures or
records). Where this document conflicts with a task spec, this document wins. Those places are
marked [OVERRIDES SPEC].

**Maintainer rulings, taken as given and not re-opened here.** (1) The auto-exit hook is the
test-copy define form: `SMBIOS_GUI_TEST_HOOKS` is set only on `smbios_gui_autoexit`, which is
built only locally, and the shipped `smbios_gui` does not contain the hook. (2) The clipboard
round trip saves and restores the user's clipboard *text*, loses non-text content (this is
documented), and runs in the normal `gui-local` tier.

## Assets & trust boundaries

- **Assets:**
  - Live firmware identifiers on the developer's machine: serials, UUIDs, asset tags, OEM strings.
  - The developer's clipboard contents and clipboard history.
  - The integrity of the claim "the shipped `smbios_gui` is unchanged".
  - CI logs, which are public for a public repo.
  - The CI tier's guarantee that it is hermetic.
- **B1 - firmware -> live `SMBios` -> local tests.** The data is hostile and identifying. It
  reaches `render_entry_as()` output, the `smbios` CLI's stdout, the OS clipboard and the twin's
  window.
- **B2 - test process -> child processes** (`smbios`, `smbios_gui_autoexit`). This boundary
  covers the command line, the working directory, pipes and lifetime.
- **B3 - test process -> OS shared state.** This means the clipboard (plus any history, sync or
  manager attached to it), the display, and files in the working directory.
- **B4 - test output -> CTest log files** (`Testing/Temporary/LastTest.log` is a file on disk)
  **-> the GitHub Actions log.** The Actions log interprets `::command::` lines.
- **B5 - build configuration -> shipped artifact.** CMake cache, presets, environment
  (`CI`/`GITHUB_ACTIONS`, `CXXFLAGS`), `-D` flags, and directory-scope definitions.

## Findings

### [MEDIUM] T-1 Shell command injection / path mishandling when launching `smbios` and the twin (CWE-78, CWE-88, CWE-427)
- Vector / evidence: Task 04 (`ClipboardRoundTripMatchesCli`, `LiveParity...`) specifies
  `popen`/`_popen`. Both go through `/bin/sh -c` or `cmd.exe /c`. The executable path comes from
  `$<TARGET_FILE:smbios>`, which embeds the build-tree path. That path can contain spaces, `'`,
  `&`, `^`, `%` or `$` (for example a home directory named `John's Mac`). `cmd.exe` quoting
  cannot be made safe in general.
- Impact: a mis-parsed command or arbitrary command execution, in the developer's session,
  controlled by the checkout path. A PATH-relative lookup could also run an unexpected `smbios`.
- Mitigation: do not use a shell. Use one small spawn helper in `tests/gui/support/`:
  - POSIX: `posix_spawn` (not `posix_spawnp`), or `fork`+`execv`, with an argv array.
  - Windows: `CreateProcessW` with `lpApplicationName` set to the absolute path, a command line
    built with the documented argv quoting rules, and `bInheritHandles` limited to the pipe
    handles.
  - Use absolute paths only, from the compile definitions, and check that the file exists as a
    regular file before spawning. Never search `PATH`.
  - Arguments come only from a fixed literal allowlist (`--format` plus `json`/`text`/`xml`/`yaml`,
    and `--exit-after-frames` plus a literal number). Never pass a registry-derived string.

### [MEDIUM] T-2 Live device identifiers pushed through the OS clipboard (CWE-359, CWE-200, CWE-226)
- Vector / evidence: Task 04 runs every registered node in 4 formats through
  `glfwSetClipboardString`. That is about 100 writes on the maintainer's Mac, which has 26
  registered nodes. Many entries contain serials or UUIDs (types 1, 2, 3, 4, 17, 11). Several
  mechanisms record every write, and the test's restore does **not** remove those records:
  - Windows Clipboard History and Cloud Clipboard (Win+V).
  - macOS Universal Clipboard/Handoff, and third-party clipboard managers (Raycast, Alfred,
    Maccy, Paste).
  - Linux clipboard managers (Klipper, GPaste).

  GLFW cannot set the opt-out markers (`ExcludeClipboardContentFromMonitorProcessing`,
  `CanIncludeInClipboardHistory`, `org.nspasteboard.ConcealedType`).
- Additional failure modes:
  - A crash, a CTest `TIMEOUT` kill or Ctrl-C skips RAII. The clipboard then still holds live
    identifier data, and the user's text is lost.
  - On X11/Wayland, clipboard content is owned by the writing process. After the test exits, the
    "restored" text disappears unless a clipboard manager keeps it.
  - If the user copies something during the run, an unconditional restore overwrites it.
  - Non-text content is lost. This is ruled and accepted.
- Impact: device identifiers persist in clipboard history or sync stores outside the build tree,
  and the user's clipboard data is lost.
- Mitigation (keeps the ruling: save/restore text, normal tier):
  - **[OVERRIDES SPEC]** Split the case into two legs:
    - (a) The byte-equality and CLI-containment check for **every** node and format runs in
      memory only. It needs no clipboard.
    - (b) The OS-clipboard leg runs once per format (4 writes) on a single entry with no
      identifier fields. Use type 0 (BIOS Information); if type 0 is absent, use the first
      registered node whose type is not 1/2/3/4/11/17. It asserts that the clipboard returns the
      exact bytes of `render_entry_as()` for that entry.

    This keeps M-09/M-10 coverage of every format through the real clipboard and removes about
    96% of the identifier writes. If the maintainer prefers all nodes on the OS clipboard, that
    is a documented accepted risk in `manual-testing.md`/`test-subsystem.md` ("writes device
    identifiers into clipboard history/sync"), and it is not the default.
  - Restore discipline:
    - The RAII guard is constructed after `glfwInit()`/window creation and destroyed before
      them.
    - Before restoring, compare the clipboard with the last value the test wrote. If they
      differ, the user changed it: do not restore, and say so in the one-line notice.
    - If the saved text was NULL (empty or non-text), restore an empty string. Never leave live
      data on the clipboard.
    - Print the one-line notice **before** the first write as well, so that an interrupted run
      leaves a trace: `clipboard in use by ClipboardRoundTripMatchesCli; restored on completion`.
    - Install no signal handlers. They would not be async-signal-safe.
  - Document the X11 ownership loss and the crash residue next to the non-text loss.

### [MEDIUM] T-3 Live or captured data printed by assertion macros into on-disk and public logs (CWE-532, CWE-117)
- Vector / evidence: Boost.Test prints both operands on failure for `BOOST_TEST(a == b)`,
  `BOOST_CHECK_EQUAL`, `BOOST_REQUIRE_EQUAL` and collection comparisons. CTest always writes test
  output to `build/<preset>/Testing/Temporary/LastTest.log`, a file that persists. That defeats
  "never written to a file". On CI, `--output-on-failure` plus `run_quiet`'s `tail -n 40` puts
  the output into the public Actions log. There, a captured hostile fixture string containing a
  newline followed by `::error::`, `::add-mask::` or `::stop-commands::` is interpreted as a
  workflow command (CWE-77, the GitHub log-injection class).
- Impact: device identifiers persist in a local log file. In CI, the log is spoofed or
  annotations are injected.
- Mitigation: any assertion that involves captured text, clipboard bytes, CLI stdout, saved
  clipboard text or `render_entry_as()` output must use the boolean form
  `BOOST_TEST(pred, "<case>: node #<i> <format>: bytes differ (expected <n> -> actual <m>)")`,
  with counts, indices, type codes and literal names only. Never compare such strings with an
  operand-printing macro. Provide a single helper (`expect_same_bytes(a, b, what)`) and use it
  everywhere. Never print `e.what()` from an exception raised in parsing or rendering. Also,
  `SKIP_REGULAR_EXPRESSION` must only ever be able to match text the test itself printed
  (see T-9).

### [LOW] T-4 Test hook leaking into the shipped `smbios_gui` (CWE-489, CWE-1188)
- Vector / evidence: the hook is gated only by `SMBIOS_GUI_TEST_HOOKS`. Leak paths:
  - `add_compile_definitions()` or `add_definitions()` at directory scope in
    `smbios_gui/CMakeLists.txt` or the root file.
  - A PUBLIC or INTERFACE definition on a library that both targets link (for example
    `libsmbios_gui_model`).
  - An OBJECT library shared by both targets so that `main.cpp` is compiled once.
  - `CMAKE_CXX_FLAGS` or the `CXXFLAGS` environment variable seeding `-DSMBIOS_GUI_TEST_HOOKS`.
  - A preset `cacheVariables` entry.
  - `SMBIOS_COMPILE_OPTIONS` (set in the root file and shared by every target).
- Impact: low. A leaked hook is exit-only: it can make the GUI quit early or refuse to start
  with exit 2. The real damage would be to the claim that the shipped binary is unchanged, and
  to M-01/M-14 test validity.
- Mitigation: see requirements R-12 to R-15. The proof has three parts:
  - A configure-time CMake `FATAL_ERROR` check.
  - A binary string count of 0 in `smbios_gui`, run **in the CI `gui-build` job on all three
    OSes** as well as locally. CI builds exactly the artifact that ships.
  - A string count of at least 1 in the twin, run locally.

  The grep must be `grep -a -c -F -e '--exit-after-frames'`. A bare `grep -c --exit-after-frames`
  parses the pattern as an option and silently checks nothing.

### [LOW] T-5 argv parsing in the twin (CWE-20, CWE-190, CWE-134)
- Vector: `main(int argc, char** argv)` exists only under the define. Risks: integer overflow in
  `atoi`/`strtol`, accepting `+5`, ` 5`, `5x`, `0x10`, an empty string or Unicode digits, and
  echoing argv into stderr.
- Mitigation: the twin requires `argc == 3`, `argv[1]` equal to `--exit-after-frames`, and
  `argv[2]` 1-6 characters long, all `'0'..'9'`, parsed with `std::from_chars` with a
  full-consumption check, range 1..100000. Anything else, including no arguments, prints exactly
  `smbios_gui: usage: --exit-after-frames <1-100000>` to stderr and exits 2. It never echoes or
  formats argv. N is a runtime argument of the twin only. There is no environment variable and
  no compile-time constant.
- Exit behaviour:
  - The counter counts only frames that pass the minimized check and reach `RenderDrawData`.
  - At N it calls `glfwSetWindowShouldClose()` and leaves through the normal guard teardown.
    It never calls `exit`, `_exit` or `std::quick_exit`.
  - Nothing else in `main` changes under the define: `IniFilename = nullptr`, swap interval,
    root banner and error paths stay identical.
  - A minimized or zero-framebuffer window blocks in `glfwWaitEvents()` forever. The parent's
    timeout handles that case (T-8).

### [LOW] T-6 Local-tier gating is not a security boundary (CWE-693)
- Evidence: `DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{CI}`, plus a forced-OFF cache value, plus
  `local_only_guard.hpp`'s `#error`.
- Analysis: in normal use the gate is fail-safe:
  - A stray `-DSMBIOS_GUI_LOCAL_TESTS=ON` is forced OFF under CI.
  - A cached ON in a reused tree is forced OFF on the next CI configure.
  - CI always configures fresh. `actions/cache` stores only vcpkg archives, never
    `CMakeCache.txt`.
  - `CI=false` still counts as "defined", which is the fail-safe direction.
  - A developer whose shell exports `CI` loses the local tier, with a warning.

  A fork PR controls `ci.yml` and the CMake files, so it can switch anything on. That is the
  code-review boundary, not this gate. The impact is small anyway: `on: [push, pull_request]`
  (not `pull_request_target`), the `gui-build` job uses no secrets, and a fork's token is
  read-only. The real proof that the local tier is absent is the CI guard. It must fail closed
  (R-8).
- Mitigation: R-1 to R-4 and R-8. Optional follow-up: add `permissions: contents: read` to the
  `gui-build` job. This is defence in depth; the other jobs are left to the CI-rework milestone.

### [LOW] T-7 Headless CI tier touching real OS state through ImGui defaults (CWE-1188)
- Evidence (pinned `imgui.cpp` 1.92.8):
  - `CreateContext` installs defaults at `:4426-4429`.
  - Windows: `Platform_{Get,Set}ClipboardTextFn_DefaultImpl` use the Win32 clipboard
    (`:15949-15999`).
  - `Platform_OpenInShellFn_DefaultImpl` calls `ShellExecuteW` on Windows and
    `fork`+`execvp("open"/"xdg-open")` elsewhere (`:16077-16120`).
  - Windows: `Platform_SetImeDataFn` uses IMM.
  - `io.IniFilename` defaults to `"imgui.ini"` (saved every 5 s and at `DestroyContext`).
  - `io.LogFilename` defaults to `"imgui_log.txt"` (`:1636`).
  - The legacy `io.{Get,Set}ClipboardTextFn` are remapped at `NewFrame` (`:10974-10977`).
- Impact: a CI test on Windows could read or write the runner's clipboard, a stray file could
  appear in the CTest working directory, and a shell could be launched if an item ever triggered
  `OpenInShell`. Today nothing in `entry_tree_view.cpp`/`copy_as_action.cpp` calls
  `TextLink*`/`OpenInShell`, and the copy path uses `ImGui::SetClipboardText`
  (`copy_as_action.cpp:102`), so the hook replacement covers it.
- Mitigation: R-9.

### [LOW] T-8 Child-process hangs, orphans and unbounded reads (CWE-400, CWE-404)
- Vector: a hidden or occluded window with vsync can stall on macOS. A minimized window blocks
  in `glfwWaitEvents`. `popen` has no timeout. A CTest `TIMEOUT` kills the test process but not
  necessarily its child, which can leave an orphaned twin window showing live data. A hostile
  table can make the CLI's stdout very large.
- Mitigation: the spawn helper enforces its own deadline (twin 30 s, CLI 30 s, both below the
  120 s CTest `TIMEOUT`) and kills and reaps the child on expiry. On Windows, a Job Object with
  `KILL_ON_JOB_CLOSE` also kills it if the test dies. Read stdout and stderr concurrently, with
  no deadlock on full pipes, up to a 64 MiB cap; past the cap, kill the child and fail with
  sizes only. Capture the CLI's stderr too, so it is never inherited into the CTest log.

### [LOW] T-9 Test-result integrity: spoofed parity and masked failures (CWE-150, CWE-74)
- Vector:
  - `LiveParity...` parses `Header type: N` lines from `--format text`. The text formatter does
    not escape firmware strings, so a string containing `\nHeader type: 99` forges a line.
  - The "0 structures" skip can turn a real GUI/CLI disagreement into a *Skipped* result.
  - `SKIP_REGULAR_EXPRESSION` matched against output that could contain uncontrolled text could
    turn a failure into a skip.
- Mitigation: parse type order from `--format json` (escaped, terminal-safe), or anchor each
  match to the exact formatter-emitted line shape and cross-check the count against the json run.
  Skip only when **both** the GUI tree and the CLI report 0 structures; any disagreement fails.
  Because live or captured text is never printed (T-3), only the test's own `SKIP:` line can
  match. Verify that the regex actually matches after Boost.Test's `Running 1 test case...`
  banner: CMake's `^` anchors to the start of the whole output. This is a functional bug
  risk, not a vulnerability.

### [LOW] T-10 ODR and hidden state in the CI tier (CWE-1041 / reliability)
- `entry_tree_view.cpp`/`copy_as_action.cpp` compiled into `smbios_gui_ci_test` are safe as
  long as they are never also in `libsmbios_gui_model`. If they were, a static-archive link could
  silently choose one copy, or fail with duplicates. ImGui config macros come from
  `imgui::imgui`'s INTERFACE properties, so both TUs share one struct layout. The function-local
  statics (`copy_as_action.cpp:70,116`, Hex flag) and the global `GImGui` persist across cases.
  `names` at `:70` is captured once, after the static-init registry fill, which is fine.
- Mitigation: R-10.

### [INFO] T-11 Residual, accepted
- The twin draws live identifiers in a visible window for about 2 s (120 frames at vsync).
  Screen sharing or recording could capture them. Hiding the window would make the twin diverge
  from the shipped binary. Accept this, and document "close screen shares before
  `ctest -L gui-local`".
- `HiddenWindowSmoke` renders live data off-screen only.
- The local tier running as root contradicts G-8/M-12. See R-19.

## Per-task requirements

**Task 01 (scaffolding)**
1. CI detection is `DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{CI}`, evaluated on **every**
   configure, not only on the first. The forced OFF uses `CACHE BOOL "" FORCE` and offers no
   override variable of any kind (T-6).
2. Add the `tests/gui/local/` subdirectory, the local test target, the twin and every `add_test`
   for them only inside `if(SMBIOS_BUILD_GUI AND SMBIOS_GUI_LOCAL_TESTS AND NOT
   SMBIOS_CI_ENVIRONMENT)`. Checking `NOT SMBIOS_CI_ENVIRONMENT` again here is defence in depth,
   in case the cached option and the force ever diverge.
3. Set `SMBIOS_GUI_LOCAL_ONLY_TESTS` only through `target_compile_definitions(<local target>
   PRIVATE ...)`. Never use directory scope, PUBLIC/INTERFACE, `SMBIOS_COMPILE_OPTIONS` or a
   preset. `local_only_guard.hpp` is the first include in every `local/*.cpp` and in
   `support/glfw_test_window.*`, `support/local_skip.hpp` and the spawn and clipboard helpers.
4. Do not add `SMBIOS_GUI_LOCAL_TESTS`, `SMBIOS_GUI_TEST_HOOKS` or `SMBIOS_GUI_LOCAL_ONLY_TESTS`
   to any `CMakePresets.json` preset.

**Task 02 (headless harness + CI wiring)**
5. Every assertion in `ci/` that involves captured text or clipboard bytes uses the boolean,
   counts-only form (T-3). No operand-printing comparison of strings. A failure message never
   includes fixture text, even synthetic text. Hostile fixtures may contain `::`, ESC and
   newlines, and the Actions log interprets them.
6. The `gui-ci` ctest step runs inside the existing `run_quiet`, with `--timeout 60`. Do not add
   any `ACTIONS_STEP_DEBUG`/verbose flag that would print passing output.
7. The CI string check (T-4) is added to `gui-build` on all three OSes, after the binary is
   found: `grep -a -c -F -e '--exit-after-frames' "$BIN"` must print 0, and so must
   `grep -a -c -F -e 'usage: --exit-after-frames'`. Otherwise print
   `::error::test hook present in shipped smbios_gui: expected 0 -> <n>` and exit 1. It can be
   added now, with 0 expected, or in Task 04, but it must exist before Task 04's `main.cpp` edit
   merges.
8. The guard that the local tier is absent fails closed. It extracts `Total Tests: <n>` from
   `ctest -N -L gui-local` and treats a missing match as a failure, not as 0. It also runs
   `ctest -N -R 'smbios_gui_local_test|smbios_gui_autoexit'`, which must report 0, in case a
   label is missing. It runs
   `find build/<preset> \( -name 'smbios_gui_local_test*' -o -name 'smbios_gui_autoexit*' \)`
   with **no** `-type f`, so `CMakeFiles/*.dir` directories also count, and that must find
   nothing. Both names are covered from Task 02 onward [OVERRIDES SPEC, which checks only
   `smbios_gui_local_test*` in Task 02].
9. `HeadlessImGuiContext`, immediately after `CreateContext()` and before the first `NewFrame()`
   (T-7):
   - Set `io.IniFilename = nullptr` and `io.LogFilename = nullptr`.
   - Set `platform_io.Platform_SetClipboardTextFn`/`Platform_GetClipboardTextFn` to in-memory
     capture.
   - Set `Platform_OpenInShellFn` to a function that records the call and returns false.
   - Set `Platform_SetImeDataFn = nullptr`.
   - Keep `io.SetClipboardTextFn`/`io.GetClipboardTextFn` (legacy) NULL.
   - After every `run_frame`, assert that the four hook pointers are unchanged and that the
     open-in-shell call count is 0.
   - Use `LogToBuffer` only. Never call `LogToTTY`, `LogToFile` or `LogToClipboard`, or the
     `Show*Window` debug tools.
   - The destructor calls `DestroyContext()` and leaves `GImGui == nullptr`.
   - A case that finds a stray `imgui.ini`/`imgui_log.txt` in the CTest working directory after
     the suite fails. Add this as a cheap check in the spike.

**Task 03 (CI-safe cases)**
10. Do not move `entry_tree_view.cpp` or `copy_as_action.cpp` into `SMBIOS_GUI_MODEL_SOURCES`.
    Add a CMake assertion (`if(... IN_LIST SMBIOS_GUI_MODEL_SOURCES) message(FATAL_ERROR ...)`)
    in `tests/gui/CMakeLists.txt` (T-10). Compile them with the same `SMBIOS_COMPILE_OPTIONS`
    and `SMBIOS_WARNING_OPTIONS` and the same `imgui::imgui` target as `smbios_gui`. Every case
    that changes a function-local static restores it, and the suite passes with `--random=1`.
11. No production seam (README). Hostile-string fixtures are synthetic and defined in the test;
    requirement 20 of the browsing-UI model applies.

**Task 04 (local tier + twin)**
12. The twin uses `target_compile_definitions(smbios_gui_autoexit PRIVATE
    SMBIOS_GUI_TEST_HOOKS=1)` only. Its own `add_executable` compiles `main.cpp` itself, with no
    OBJECT library shared with `smbios_gui`. It has no install rule and its output directory is
    under the test bin directory. It is created only under R-2's condition (T-4).
13. Add a configure-time guard in `smbios_gui/CMakeLists.txt`, after `smbios_gui` is defined.
    If `SMBIOS_GUI_TEST_HOOKS` appears in `smbios_gui`'s `COMPILE_DEFINITIONS`, the directory's
    `COMPILE_DEFINITIONS`, `CMAKE_CXX_FLAGS`, `CMAKE_CXX_FLAGS_<CONFIG>` (all configs),
    `SMBIOS_COMPILE_OPTIONS` or `libsmbios_gui_model`'s `INTERFACE_COMPILE_DEFINITIONS`, fail
    with `FATAL_ERROR`. This does not change `smbios_gui`'s compile line.
14. Hook shape exactly per T-5:
    - `argc == 3`, a literal flag, 1-6 ASCII digits, `from_chars` with full consumption,
      range 1..100000.
    - Any other argv, including none, gives the fixed usage line and exit 2. Never echo argv.
    - Exit only through `glfwSetWindowShouldClose` and normal teardown.
    - No environment read and no other behaviour change.
    - The `#else` branch keeps `int main()` byte-identical to today.
    - Keep the hook code in at most two `#if defined(SMBIOS_GUI_TEST_HOOKS)` blocks (argument
      parse and per-frame counter) plus the signature switch.
15. Inertness proof, recorded in `status.md` as counts only:
    - The R-7 grep on `smbios_gui` gives 0. This is checked on CI on all three OSes and locally.
    - The same grep on `smbios_gui_autoexit` gives at least 1, checked locally.
    - With Ninja, `ninja -C build/macos-gui -t commands smbios_gui | grep -c SMBIOS_GUI_TEST_HOOKS`
      gives 0.
16. Spawning goes through one helper, with no shell, no `popen`/`_popen`/`system`, no PATH
    search, absolute paths from `SMBIOS_CLI_PATH`/`SMBIOS_GUI_AUTOEXIT_PATH`, and a fixed
    literal argv allowlist (T-1). Set the twin's working directory through the spawn API
    (`fork`+`chdir`+`execv`, or `CreateProcessW`'s `lpCurrentDirectory`). **Never** change the
    test process's own current directory. The helper enforces a 30 s deadline with kill and
    reap, uses a Windows Job Object, drains stdout and stderr concurrently under a 64 MiB cap,
    and checks the exit status (T-8).
17. The twin's temporary directory comes from `mkdtemp` (mode 0700) on POSIX, or on Windows from
    a fresh, uniquely named directory under `GetTempPath2W`/`temp_directory_path()`, created
    with `create_directory` and retried if it already exists. Report "not empty" as an entry
    count only. Remove only that directory, after the child has been reaped.
18. Clipboard, per T-2:
    - [OVERRIDES SPEC] Use the in-memory all-nodes leg plus a 4-write OS leg on one entry with
      no identifier fields. Otherwise the maintainer must record the accepted risk.
    - The RAII guard lives inside the GLFW lifetime.
    - Compare before restoring. A NULL saved value restores to empty.
    - Print the notice before the first write and after the restore.
    - Saved text is never printed, logged or written to a file.
    - Document the crash residue, the X11/Wayland ownership loss and clipboard-history
      recording in `manual-testing.md`, next to the non-text loss.
19. If `geteuid() == 0` on POSIX, every local case skips with
    `SKIP: <case>: local GUI tests do not run as root`. No skip line or message suggests
    `sudo` or elevation (G-8, M-12).
20. Assertions follow T-3 (the boolean form, through one helper). Live parity follows T-9:
    parse json or exact line shapes, and skip only if both sides report 0. Output is limited to
    `<what>: expected -> actual` lines with counts, indices and type codes, plus the extras
    count line and the clipboard notice. No serial, UUID, asset-tag or OEM string appears
    anywhere.

**Task 05 (docs + exit gate)**
21. Gate item 4 cites the R-8 guard and the R-7 string check for each OS. Gate item 5's grep
    also checks that no `tests/gui/` source contains `popen`, `_popen`, `system(` or
    `posix_spawnp`, and that `SMBIOS_GUI_TEST_HOOKS` appears only in `smbios_gui/src/main.cpp`
    and in the twin's `target_compile_definitions` line.
22. `CLAUDE.md` and `test-subsystem.md` state that the twin exists only for local GUI builds and
    is not an app. `manual-testing.md` states the clipboard caveats (R-18) and "close screen
    shares before running `gui-local`" (T-11). No document suggests root for any GUI test.

## Verdict: PASS_WITH_FOLLOWUP

There are no CRITICAL or HIGH findings, and nothing blocks the start of implementation,
including the `main.cpp` edit in Task 04, provided R-12 to R-16 are followed. Merge-blocking at
code review:
- any shell-based spawn (R-16);
- any operand-printing assertion on live or captured data (R-5, R-20);
- a nonzero hook string count in the shipped `smbios_gui` on any OS (R-7/R-15);
- a local-tier artifact found on CI (R-8).

Fixes go to `cpp-expert` (R-9, R-12 to R-14, R-16). Harness and guard tests go to
`testing-expert` (R-5, R-8, R-10, R-17 to R-20). Follow-up, not blocking: add
`permissions: contents: read` to `gui-build` (T-6).
