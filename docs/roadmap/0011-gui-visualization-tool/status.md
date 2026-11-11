# Milestone 0011 - GUI Visualization Tool - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                | Status         | Tests |
|-------|----------------------------------------------------------|----------------|-------|
| 01.0  | App shell: windowing, GPU backend and build target      | ✅ Complete | macOS build + unit suite; 4s launch smoke |
| 02.0  | SMBIOS browsing UI: entry tree and field table           | ✅ Complete | 676 unit cases (40+2 new GUI), ASan/UBSan clean; parity 26 = 26 |
| 03.0  | Cross-platform build integration                         | 🔶 In progress (2/3 tasks) | - |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- 2026-11-11 R3/R4: no GUI code exists (stories absent); decomposition audit found no gaps (10 task specs match the README rows). No scope extension.
- 2026-11-11 01.0/Task 01: imgui feature names `glfw-binding`/`opengl3-binding` confirmed against pinned baseline port (imgui 1.92.8, glfw3 3.5.1); no correction needed. Verified on macOS only; Windows/Linux preset configure not run on this host - to be covered by Story 03.0 CI.
- 2026-11-11 01.0/Task 02: spec corrected - `CMAKE_RUNTIME_OUTPUT_DIRECTORY` is `"bin"` (not `"../bin"`; binary lands in `<build>/smbios_gui/bin/`). Additions beyond spec: `GL_SILENCE_DEPRECATION` (Apple, needed under -Werror), macOS GL 3.2 core/forward-compat hints + `#version 150`, RAII teardown guards, `IniFilename=nullptr`. imgui::imgui / glfw target names confirmed. nm confirms whole-archive link on macOS. UNVERIFIED: manual window run (no display in loop - maintainer to confirm open/placeholder/Escape/exit 0), Windows/Linux builds (-> Story 03.0 CI).
- 2026-11-11 01.0 /pr-review (feature: REQUEST_CHANGES, security: PASS_WITH_FOLLOWUP): ratified by the maintainer - the GUI build is **opt-in**: `option(SMBIOS_BUILD_GUI OFF)` guards `add_subdirectory(smbios_gui)`, and imgui/glfw3 move to a vcpkg manifest feature `gui` (enabled with `-DVCPKG_MANIFEST_FEATURES=gui`). This supersedes the specs' "unconditional, no opt-out flag" wording (Task 01 manifest snippet, Task 02 root CMakeLists bullet). Reason: unconditional deps force X11/GL dev packages onto every Linux/CI build and CLI-only contributors; ci.yml has no apt step. Consequence for Story 03.0: its CI wiring must add a dedicated GUI build job (with Linux apt packages) and `-DSMBIOS_BUILD_GUI=ON -DVCPKG_MANIFEST_FEATURES=gui`; its "no other change to CI" assumption no longer holds - amend 03.0 specs before starting it.
- 2026-11-11 Story 02.0 security follow-ups (from 01.0 security review): never pass firmware strings as a format string (`TextUnformatted`/`"%s"` only, `PushID` for label collisions); bound/cap rendered string length; add a `%n%s`-style hostile-string test; never require or suggest running the GUI as root (consider a euid==0 warning).

### Story 01.0 - App shell: windowing, GPU backend and build target (✅ 2026-11-11)

**Delivered**
- `vcpkg.json`: `imgui` (`glfw-binding`, `opengl3-binding`; 1.92.8) + `glfw3` (3.5.1) under a non-default manifest feature `gui`; baseline pin unchanged.
- `smbios_gui/` target: GLFW + OpenGL3 + Dear ImGui placeholder window, Escape closes, RAII reverse-order teardown, minimised-window guard, `imgui.ini` disabled; `libsmbios` whole-archive linked; empty `include/smbios/gui/` kept via `.gitkeep`.
- Opt-in build: `option(SMBIOS_BUILD_GUI OFF)`, hidden `gui` mixin + `windows-gui`/`linux-gui`/`macos-gui` presets, configure-time hint when the vcpkg feature is missing, CLAUDE.md "Building the GUI (opt-in)" note.

**Tests / gate**
- macOS: default configure/build installs no imgui/glfw, 21/21 ctest pass; GUI preset build clean under `-Werror`, `nm` shows libsmbios entry/formatter symbols (whole-archive works); 4s launch smoke test alive with no stderr.
- `/verify-task` PASS (Tasks 01, 02). `/pr-review`: feature LGTM, security PASS (after one REQUEST_CHANGES round fixed by the opt-in ruling).
- **Unverified (open):** clean Escape/close exit with status 0 (maintainer manual run); Windows and Linux builds (Story 03.0 CI).

**Reconciliation note**
- Task 01 spec: *consumed*, manifest shape *superseded* by the opt-in `gui` feature (see amendment in the spec).
- Task 02 spec: *consumed*; "unconditional `add_subdirectory`" *superseded* by `if(SMBIOS_BUILD_GUI)`; output dir corrected to `"bin"` (as `set_target_properties`).
- Task 03 spec: *consumed* (no new test source, per its own text).
- Story 03.0 Tasks 01-02: *amended* (GUI CI job + Linux apt packages now required).
- 2026-11-11 02.0 threat model: `docs/security/2026-11-11-gui-browsing-ui.md` (PASS_WITH_FOLLOWUP; HIGH G-1 firmware string reaching an ImGui format/label/ID = BLOCK at review). It is a standing input to every 02.0 coder briefing and **overrides** the task specs where they conflict: (a) firmware strings are sanitised for *display* only via an ImGui-free `display_text()` (view model and clipboard keep raw bytes) - supersedes the README's "render as-is"; (b) Task 02's empty-view check uses `nodes.empty()`, not `get_structures_count()==0`; (c) Task 03's Copy action must own its entries (no `create()` per frame) and exposes an ImGui-free `render_entry_as(entry, name)` with a 1 MiB cap, default format `json`. Requirements 1-20 are keyed to Tasks 01-04 in that doc.
- 2026-11-11 02.0/Task 01 (PASS): view model delivered as always-built `libsmbios_gui_model`; added `display_text()`, `decode_failed` per threat model. Spec fixes: `std::distance` on `SMBios` iterators does not compile (use a manual loop); (an earlier 'enum has 43 names' claim was WRONG - it has 46, see below). Carry-over for Tasks 03/04: put `render_entry_as()` in the model library; test target links `libsmbios_gui_model`.
- 2026-11-11 02.0/Task 02 (PASS): rendering delivered; G-1 (HIGH) audited clean by coder, orchestrator grep and verifier independently. Owed before story close: manual parity vs `smbios --format text/json` + live check of all four FieldValue kinds (spec success criteria 1-3 are `?`). Verifier could not confirm 'default build has 0 imgui refs' from `build/macos` (stale GUI cache); the coder proved it in a fresh dir.
- 2026-11-11 02.0/Task 03 (PASS): Copy-as delivered; GUI dropdown has 4 formats (no `csv`: csv formatter is CLI-target-only) - spec list corrected. Signature changed (entry created in click handler, threat-model req 11). Owed: manual clipboard paste check vs `smbios --format json`. Task 04 note: test target must whole-archive-link libsmbios for formatter registration.
- 2026-11-11 02.0/Task 04 correction: `SMBios::SMBiosHandler` has **46** named values (the Task 04 label test enumerates all 46 and checks types 0-255); the '43' figure recorded above after Task 01's verification was wrong and is withdrawn.
- 2026-11-11 **Parity gate ruling (maintainer, AskUserQuestion):** on the live table `smbios --format text` printed 26 'Header type' lines and `build_entry_tree()` produced 32 nodes. The 6 extras are Apple OEM types (0x80, 0x82, 0x83, 0x85, 0x86) with no registered entry class: the CLI's `render_document()` skips unregistered types, while the GUI keeps them by design (GC1 / story README: 'unregistered type is still a node'). Ruled: the exit gate compares **registered-type nodes only** - their count and per-node type order must equal `smbios --format text` (and `--format json`); unregistered structures are extra GUI-only nodes labelled `Unknown (0xNN)`. Result on this Mac: 26 = 26, no order or decode differences, no node failed to decode. Supersedes the 'same structure count' wording in `plan.md` (Rationale + dependency-graph exit node) and Story 02.0 Task 04's parity criterion. The CLI's output is unchanged (milestone 0009's byte-identical text format is preserved).
- 2026-11-11 **Testing-scope call (restated so no one re-raises "where are the UI tests?"):** Dear ImGui is immediate-mode, so there is no persistent widget tree or accessibility surface to inspect headlessly, and `smbios_gui`'s frame loop needs a real display/GPU that CI runners lack. Therefore the data/view-model layer (`libsmbios_gui_model`: `build_entry_tree`, `entry_type_label`, `display_text`, `field_value_text`, `entry_node_label`, `render_entry_as`, `create_entry_for_node`) is unit-tested hermetically in `smbios_unit_test` (40 cases, ASan/UBSan-clean, runs in default CI with no imgui), while ImGui rendering, window/input handling and the clipboard round-trip are verified by manual run only. No pixel or visual-regression tests (plan.md Non-goals).

### Story 02.0 - SMBIOS browsing UI: entry tree and field table (✅ 2026-11-11)

**Delivered**
- `libsmbios_gui_model` (always-built, imgui-free static library): `build_entry_tree()` / `EntryNode` / `FieldRow` (GC1) with per-entry exception isolation and `decode_failed`; `entry_type_label()`; `display_text()` (display-only sanitising, 4096-byte cap, bounded per-call cost); `field_value_text()` / `entry_node_label()`; `render_entry_as()` (1 MiB + NUL refusal, byte-exact `IOutputFormatter::render()` output) and `create_entry_for_node()`.
- `smbios_gui` executable (opt-in): left node list + right field table (ImGuiListClipper, `PushID(index)`, all labels literal, firmware text only via `display_text()` + `TextUnformatted`), hex/decimal toggle, disabled-checkbox booleans, `[decode failed]` marker, visible empty-table message, POSIX root banner, "Copy as..." dropdown (json default; json/text/xml/yaml - no csv) writing to the OS clipboard.
- `SMBios` + tree built once before the frame loop in `try/catch` (fixed stderr message, exit 1). Root `CLAUDE.md` GUI bullet extended.

**Tests / gate**
- `smbios_unit_test`: 674 -> 676 cases (42 new in `GuiViewModelUnitTests`/`GuiFieldTextUnitTests`/`GuiCopyAsUnitTests`), green; unit target now links `libsmbios_gui_model` and whole-archive-links `libsmbios`. ASan/UBSan (CI options) clean on the GUI suites. Full ctest 21/21 on macOS. `macos-gui` build clean under `-Werror`; 4 s smoke launch alive with empty stderr.
- `/verify-task` PASS (Tasks 01-04). `/pr-review`: feature LGTM, security PASS_WITH_FOLLOWUP (20/20 threat-model requirements verified; G-1 HIGH closed). Review fix P-1 (unbounded per-frame `display_text` cost) applied and proven by a bounded-cost test that fails against the old implementation.
- **Parity gate (ratified ruling):** registered-type nodes match `smbios --format text`/`json` - 26 = 26 on this Mac, order and decode identical; the GUI shows 6 extra Apple OEM `Unknown (0xNN)` nodes by design.
- **Unverified / owed (maintainer):** a person looking at the real window (node list, field table, hex toggle), a Copy-as `json` paste compared with `smbios --format json`, and clean Escape/close exit status 0. Windows/Linux builds are deferred to Story 03.0.

**Reconciliation note**
- Task 01 spec: *consumed*; superseded where the threat model added `display_text()`/`decode_failed`; `std::distance` on `SMBios` iterators is invalid (manual loop). `SMBiosHandler` has 46 values (the "43" note was wrong and is withdrawn).
- Task 02 spec: *consumed*; "empty view = `get_structures_count()==0`" *superseded by* `nodes.empty()`; strings rendered display-sanitised, not "as-is".
- Task 03 spec: *consumed*; entry-pointer signature *superseded by* `(SMBios&, const EntryNode&, size_t)` (entry created in the click handler); dropdown has 4 formats (csv is CLI-target-only).
- Task 04 spec: *consumed*; parity criterion *superseded by* the registered-nodes-only ruling.
- `display_text()` truncation count N now counts **input** bytes not shown (it was output bytes after sanitising) - the bounded-cost fix; one assertion in `DisplayTextCapAfterReplacementGrowthTestCase` was re-expressed in input bytes, none weakened.

**Follow-ups (not blocking)**
- P-2 (LOW, pre-existing, `libsmbios`): `json_escape` leaves bidi controls (U+202A-202E, U+2066-2069) unescaped - consider `\u` escaping so the "terminal-safe" json default also resists text-direction spoofing; affects the CLI too.
- P-3 (info): `render_entry_as()` renders the whole entry before applying the 1 MiB cap (brief memory spike on click).
- Cosmetic: "Copied to clipboard." status reappears when re-selecting a node; sanitiser omits zero-width/implicit bidi marks (U+200B-U+200F, U+061C); `noexcept` + `bad_alloc` termination should be stated in the Doxygen.
- Root `CLAUDE.md` still says the GUI is "never built in CI" - correct it when Story 03.0 adds the GUI CI job.
- 2026-11-11 03.0/Task 01: macOS clean-checkout `macos-gui` configure+build verified (no change needed). Windows/Linux preset builds unverifiable locally (no host/Docker) - open until Task 02's CI job runs on real runners. Linux package list taken from the vcpkg glfw3 port message: libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config.
- 2026-11-11 03.0/Task 02 (PASS as written): dedicated build-only `gui-build` CI job added (3-OS, `<os>-gui` presets, Linux apt packages from the glfw3 port message, bounded failure output, no run step); CLAUDE.md GUI bullet corrected. UNVERIFIED until a real push: job behaviour on runners, Windows/Linux gui presets, broken-main.cpp-fails check. Failure-grep pattern tightened by the orchestrator (benign file names matched the coder's pattern).

