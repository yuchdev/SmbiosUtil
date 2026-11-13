# Test Subsystem

What this project actually tests today, grouped by suite - and what the
roadmap commits to testing next. Each group below names its **type**, what
it **covers**, and **why it matters** (the failure class it guards
against). For exact assertions, read the suite itself; this document stops
at the group level on purpose.

## Taxonomy

Four Boost.Test categories plus one plain-executable category (C ABI,
milestone 0007), each its own top-level directory, all wired into
`ctest` via `enable_testing()` in the root `CMakeLists.txt`:

| Category    | Directory             | Target                     | Needs root/admin? | Always expected green? |
|--------------|------------------------|------------------------------|--------------------|--------------------------|
| Unit         | `tests/unit/`          | `smbios_unit_test`          | No                 | Yes |
| Mock         | `tests/mock/`          | `smbios_mock_test`          | No                 | Yes (currently a documented placeholder - see below) |
| Perf         | `tests/perf/`          | `smbios_perf_test`          | Yes (real hardware) | No - fails cleanly without privileges, not a regression |
| Integration  | `tests/integration/`  | `smbios_integration_test`  | Yes (real hardware) | No - same as Perf |
| C ABI        | `tests/c_abi/`         | `smbios_c_compile_as_c_test`, `smbios_c_example`, `smbios_c_open_close_test`, `smbios_c_entry_iteration_test`, `smbios_c_typed_getters_test` | No (reads real hardware, but no elevated privilege needed - same as the native sources `smbios_c` sits on top of) | Yes - each checks `smbios_open()`'s own return code and degrades gracefully rather than failing when no table is available |

**GUI (`smbios_gui`, milestone 0011).** Its ImGui-free view-model layer (`libsmbios_gui_model`) is unit-tested in `tests/unit/gui_*_test.cpp`; the rendering code (`render_entry_browser()`, `render_copy_as_action()`) is also exercised headlessly by the CI-safe tier below; a real window, the OS clipboard and the live table are exercised by the local-only tier below (developer machine only); visual layout, window behaviour, the Escape/title-bar close and the root banner stay in the manual plan in [manual-testing.md](manual-testing.md), whose *Automation status* table maps each manual check M-01..M-14 to the case that automates it. Story 04.0 adds two GUI test tiers under `tests/gui/`, both only configured when `SMBIOS_BUILD_GUI=ON` (a non-GUI build adds no target and no test). Run them with `ctest -L gui-ci`, `ctest -L gui-local` or `ctest -L gui`:

| Category | Directory | Target / CTest name(s) | CTest labels | Needs root/admin? | Always expected green? |
|----------|-----------|--------------------------|----------------|--------------------|--------------------------|
| Unit (GUI, CI-safe) | `tests/gui/ci/` | `smbios_gui_ci_test` (one CTest entry) | `gui;gui-ci` | No (no window, GL or OS clipboard; synthetic tables only) | Yes - never skips; runs in CI's `gui-build` job on all three OSes |
| Integration (GUI, local-only) | `tests/gui/local/` | `smbios_gui_local_test`, one CTest entry per case: `smbios_gui_local_test.<CaseName>`; plus the hook inertness checks `smbios_gui_autoexit.InertBinaries` / `.InertCompileLines` (`cmake -P tests/gui/support/hook_inertness_check.cmake`) | `gui;gui-local` | No - every case *skips* when run as root (`SKIP: <case>: local GUI tests do not run as root`) | Yes, or *Skipped* when a display/GL/clipboard/table is missing or when run as root - never on CI. Verified on macOS only; the Windows branches of the spawn helper and temp directory are unverified (never compiled), the Linux run is pending |

The local-only tier is controlled by `SMBIOS_GUI_LOCAL_TESTS` (default ON for a GUI build outside CI). When the `GITHUB_ACTIONS` or `CI` environment variable is set at configure time it is forced OFF with a warning, and `tests/gui/CMakeLists.txt` re-checks the CI environment where the targets are created, so under CI the tier is not configured, compiled or registered. CI's `gui-build` job proves it, failing closed: `ctest -N -L gui-local` and `ctest -N -R 'smbios_gui_local_test|smbios_gui_autoexit'` must both print `Total Tests: 0` (a missing line is a failure), and no `smbios_gui_local_test*`/`smbios_gui_autoexit*` file or directory may exist in the build tree. The same job checks that the shipped `smbios_gui` contains the test-hook string `--exit-after-frames` 0 times (`grep -a -c -F -e`). Every local-only source also fails to compile unless the target-private definition `SMBIOS_GUI_LOCAL_ONLY_TESTS` is set (`tests/gui/support/local_only_guard.hpp`). A missing resource makes a local case print one `SKIP: <CaseName>: <reason>` line (`skip_unless()`, `tests/gui/support/local_skip.hpp`), which the CTest property `SKIP_REGULAR_EXPRESSION "(^|\n)SKIP: "` reports as *Skipped*. (`"^SKIP: "` does not work: CMake anchors `^` to the start of the whole output, which begins with Boost.Test's `Running 1 test case...` line.)

**The `smbios_gui_autoexit` test twin (Story 04.0 Task 04) is not an app.** It exists only in a local GUI build (behind the same fail-closed `SMBIOS_BUILD_GUI AND SMBIOS_GUI_LOCAL_TESTS AND NOT SMBIOS_CI_ENVIRONMENT` condition, defined in `smbios_gui/CMakeLists.txt`), is written to `<build>/libsmbios/bin/`, has no install rule, and is the shipped `smbios_gui` built from the same sources and settings plus the PRIVATE definition `SMBIOS_GUI_TEST_HOOKS=1`. That define compiles in `main.cpp`'s auto-exit hook: exactly `--exit-after-frames N` (N 1-6 ASCII digits, 1..100000) closes the window after N rendered frames through the normal teardown (exit 0); any other command line prints the fixed line `smbios_gui: usage: --exit-after-frames <1-100000>` and exits 2. The shipped `smbios_gui` never gets the define: `smbios_gui/CMakeLists.txt` fails the configure if `SMBIOS_GUI_TEST_HOOKS` appears in `smbios_gui`'s or its directory's definitions/options, `CMAKE_CXX_FLAGS`/`CMAKE_CXX_FLAGS_<CONFIG>` (including a `CXXFLAGS` seed), `SMBIOS_COMPILE_OPTIONS`/`SMBIOS_WARNING_OPTIONS`, or the INTERFACE definitions/options of the libraries it links (threat model R-13). **Known limits of that configure-time guard, backstopped by `smbios_gui_autoexit.InertCompileLines`** (which inspects the real compile lines): the deferred check runs at root directory scope, so it cannot read the INTERFACE definitions/options of imported targets (e.g. `imgui::imgui`, `glfw`) that are not visible there; it cannot see a per-directory `CMAKE_CXX_FLAGS` set after its first (non-deferred) check; and it does not follow transitive INTERFACE definitions of `libsmbios`'s own dependencies (only the libraries `smbios_gui` links directly). `smbios_gui_autoexit.InertBinaries` checks `grep -a -c -F -e` counts of `--exit-after-frames` and `usage: --exit-after-frames`: 0 in the shipped binary, >= 1 in the twin; `smbios_gui_autoexit.InertCompileLines` checks that `ninja -t commands smbios_gui` (and smbios_gui's `compile_commands.json` lines, if exported) carry `SMBIOS_GUI_TEST_HOOKS` 0 times and the twin's >= 1 times (skips on a non-Ninja generator). Both fail closed on a missing binary, `grep` or `ninja`.

**Child processes in the local tier** go only through `spawn_and_capture()` (`tests/gui/support/process_spawn.{hpp,cpp}`, threat model R-16): no shell and no PATH search (POSIX `fork` + `execv`; no shell, no libc process helpers that search PATH), no inherited descriptors (the POSIX child closes every descriptor above 2, except the close-on-exec exec-error pipe, before `execv` and resets SIGPIPE to its default; Windows inherits only the three standard handles), an absolute executable path that must be an existing regular file, and arguments that are compile-time literals (`LiteralArg` has a `consteval` constructor, so a runtime string does not compile; the paths come from the compile definitions `SMBIOS_GUI_AUTOEXIT_PATH`/`SMBIOS_CLI_PATH`). The child's working directory is set in the child (`chdir` after `fork`; `lpCurrentDirectory` on Windows), never in the test process. A 30 s deadline kills (whole process group) and reaps the child; stdout and stderr are drained together into memory, capped at 64 MiB in total (past the cap the child is killed); stdin is the null device. The result (status, exit code, both outputs, elapsed time) stays in memory and is never printed: assertions report status names, exit codes and byte counts only. `ScopedTempDirectory` (`support/scoped_temp_directory.*`, `mkdtemp`, mode 0700, R-17) gives the twin a fresh working directory; `display_and_gl_available()` (`support/display_probe.*`) opens and closes a hidden GLFW window with `main.cpp`'s context hints to decide the display skip. **Windows caveat: the Windows branches of `process_spawn.cpp` (`CreateProcessW` + Job Object with `KILL_ON_JOB_CLOSE`, reader threads, `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`) and `scoped_temp_directory.cpp` are UNVERIFIED - never compiled or run, because the local-only tier is never built on CI and has only been run on macOS.** On Linux the POSIX branch is the macOS one, also not yet run there.

**CI scripts (milestone 0014) - a separate, non-CTest category.** `scripts/ci/tests/` holds Python `unittest` suites for the CI step runner (`scripts/ci/ci_step.py`), its reporter backends and the neutral environment names. They are stdlib only and need no build, hardware or network. Run them all with `python3 scripts/ci/run_tests.py`, which prints one RC1 line. They are not registered with `ctest`, and CI's `lint` job runs them once 0014 Story 02.0 wires that job.

| Category | Directory | Entry point | Needs root/admin? | Always expected green? |
|----------|-----------|-------------|--------------------|--------------------------|
| CI scripts | `scripts/ci/tests/` | `python3 scripts/ci/run_tests.py` (`unittest discover`) | No | Yes. Shell-flag cases skip with one line when no `bash` is found. |

**No `e2e` category.** A hardware-inventory library/CLI has no user journey
distinct from "construct an `SMBios`, read its structures" - that journey
*is* the integration suite. Adding a separate `e2e` layer here would just
duplicate it under a different name.

Framework is Boost.Test (`BOOST_AUTO_TEST_SUITE`/`BOOST_AUTO_TEST_CASE`)
throughout, matching the vcpkg-declared `boost-test` dependency. Run one
suite directly for a focused check: `./smbios_unit_test
--run_test=<SuiteName> --log_level=message`; run everything via `ctest
--test-dir build/<preset> --output-on-failure`.

**Known gap in the GUI local-only tier:** the intent is that a `gui-local` case SKIPs (one `SKIP:` line) when a resource is missing, but `HiddenWindowSmoke` and `AutoExitTwinSmoke` currently FAIL instead of skipping when the SMBIOS table cannot be read (relevant on an unprivileged Linux run, where the sysfs table is typically root-only). The tier has only been run on macOS. See `docs/dev/cpp_coding_standard.md` and Story 04.0 Task 04's follow-up.

## Keeping this registry current

This document is the registry of automated tests. **Whenever an automated test
is added, renamed, split, removed or moved to another category, update this
file in the same change**: add or adjust the suite's row under *Implemented
coverage* (suite name, file, case count, what it covers, what it guards
against), move any matching item out of *Intended coverage*, and update the
taxonomy table if a new executable, directory or CTest label appears. A quick
self-check, limited to what your change touches: list the suites in the test
files you added or modified (`git diff --name-only HEAD -- tests` plus any new
files, then `grep -hoE 'BOOST_AUTO_TEST_SUITE\(\w+\)'` on them) and confirm each
`<Name>` and its case count appear here. (Older per-structure-type suites such
as `<Type>EntryUnitTests` are covered only at group level above; backfilling
individual rows is welcome but is not required by an unrelated change.) Manual-only checks live in the separate manual plans
(for the GUI: [manual-testing.md](manual-testing.md)) and are linked, not
duplicated, here.

## Implemented coverage

### Unit - `tests/unit/` (`smbios_unit_test`)

Deterministic, hand-built byte buffers and `DMIHeader`s - no real hardware,
no privileges, always expected to pass. This is where every parsing rule
and every regression fix lands first.

| Suite | Covers | Guards against |
|-------|--------|------------------|
| `SmbiosUnitTests` (`unit_tests.cpp`) | `SMBiosVersion::operator>`/`operator<` lexicographic comparison; `PortConnectionEntry` parsing from a synthetic buffer; `SMBiosEntryFactory`'s type→class lookup (including the unregistered-type `nullptr` case); `SMBiosImpl::get_table_size()` against an injected buffer | The historical version-comparison bug (`{2,9} > {3,0}` evaluating wrong) silently corrupting which versioned fields a caller thinks are present; the factory returning the wrong entry class or crashing on an unknown SMBIOS type; a table-size regression (the milestone 0002 missing-`return` defect) |
| `ByteReaderUnitTests` (`byte_reader_test.cpp`) | `smbios::read_u8/u16/u32/u64` - little-endian byte composition, `constexpr`-verified via `static_assert` | A naive `reinterpret_cast`-style read silently reading multi-byte SMBIOS fields in host byte order instead of the DMTF spec's little-endian wire format - the exact class of bug milestone 0003 exists to eliminate |
| `BiosInformationEntryUnitTests` (`bios_information_entry_test.cpp`) | Every `BiosInformationEntry` field accessor across the three real length tiers (`< 0x12`, `0x12`-`0x18`, `> 0x18`), including the newly-added `get_extended_rom_size()` | A length-guard regression silently reading past a truncated structure's declared bounds, or a field decoding at the wrong byte offset (two such offset bugs were found and fixed by this exact suite's sibling, see Memory Device below) |
| `MemoryDeviceEntryUnitTests` (`memory_device_entry_test.cpp`) | Every `MemoryDeviceEntry` field accessor across all five legacy length tiers, including the five previously-unexposed getters (`get_extended_size()`, `get_memory_clock_speed()`, three voltage fields) | The real latent out-of-bounds read closed in milestone 0003 (the old `MemoryDeviceV23` pointer-overlay technique could read 4 trailing fields past a structure's actual length for `0x17 <= length < 0x1B`) and two doc-comment offset bugs (`get_array_handle()`, `get_device_rank()`) this suite caught by exercising hand-computed expected values at each tier |
| `SmbiosTableWalkUnitTests` (`smbios_table_walk_test.cpp`) | `SMBios::read_smbios_table()`/`count_smbios_structures()`'s bounds checks against a truncated last structure, an exact-boundary-fitting header, and an empty buffer | An out-of-bounds read past the mapped SMBIOS table on a malformed/truncated input - confirmed via a real pre-fix/post-fix behavior difference, with AddressSanitizer (the CI `sanitizers` job) as the independent proof this suite's assertions alone can't fully provide locally |
| `DocSamplesTests` (`doc_samples_test.cpp`) | `GoldenOutputsMatchFixtureRender`: rendering the synthetic doc-sample fixture through each built-in formatter matches the golden files in `tests/data/output_formats/`; `FixtureCoversEveryFieldValueAlternative`: the fixture exercises every `FieldValue` alternative | Doc samples (and `scripts/check_doc_samples.py`'s embedded excerpts) drifting from what the formatters really emit; a new `FieldValue` alternative that no sample covers |
| Formatter suites (`output_formatter_registry_test.cpp`, `built_in_formatter_registry_test.cpp`, `text_`/`xml_`/`json_`/`yaml_`/`csv_formatter_test.cpp`, `formatter_escaping_test.cpp`) | `OutputFormatterRegistryUnitTests`, `BuiltInFormatterRegistryUnitTests`, `{Text,Xml,Json,Yaml,Csv}FormatterUnitTests`, `FormatterEscapingUnitTests`: registry lookup/duplicate rules, each format's layout, and per-format escaping | A formatter silently changing its output shape, unescaped control or markup characters corrupting a document, a built-in format failing to register |
| `GuiViewModelUnitTests` (`gui_view_model_test.cpp`, 17 cases) | `smbios::gui::build_entry_tree()` / `entry_type_label()` over `SyntheticTableBuilder` tables: node count equals the walk count (registered, unregistered and OEM types), fields equal a direct `get_fields()`, `Unknown (0xNN)` labels for all 46 `SMBiosHandler` values and unnamed types, hostile strings (`%n%s%x`, `##`, ESC, bidi, invalid UTF-8, 64 KiB) kept raw, five malformed-table shapes plus 500 fixed-seed random tables, duplicate handles stay distinct nodes, empty / end-of-table-only tables, a throwing entry becoming a `decode_failed` node, `bad_alloc` propagating, nodes outliving the `SMBios` | A hostile or malformed table making the GUI crash, skip a structure, merge two structures with the same handle, or show firmware text unsanitised; the view model silently disagreeing with the entry classes' own `get_fields()` |
| `GuiFieldTextUnitTests` (`gui_field_text_test.cpp`, 15 cases) | `display_text()` (display-only sanitising: C0/DEL/C1/bidi replacement edges, embedded NUL, 4096-byte cap incl. suffix, never splitting a code point, idempotence, bounded cost on a 16 MiB input), `field_value_text()` (string / number decimal+hex / bool / byte-vector hex dump with its 1024-byte cap) and `entry_node_label()` | A firmware string reaching ImGui with control or direction-override characters, a multi-megabyte string slowing every frame (the P-1 finding - the bounded-cost cases fail against the old implementation), a hex dump or label growing without bound |
| `GuiCopyAsUnitTests` (`gui_copy_as_test.cpp`, 10 cases) | `render_entry_as()` for every registered formatter (byte-exact `IOutputFormatter::render()` output, including hostile strings; unknown / empty / case-variant names and over-1 MiB output give `nullopt`; exactly 1 MiB allowed), `default_copy_format_index()`, `is_terminal_safe_format()`, `create_entry_for_node()` (index/type/handle re-check, duplicate handles resolved by index) | The "Copy as..." clipboard text diverging from the CLI's formatter output, copying the wrong structure after a stale selection, an oversized copy or a terminal-unsafe default format |

### GUI tiers - `tests/gui/` (`smbios_gui_ci_test`, `smbios_gui_local_test`, `smbios_gui_autoexit.*`)

**CI-safe tier: headless spike (Story 04.0, Task 02) and widget tests
(Task 03); local tier: Task 04 (auto-exit twin, hidden-window smoke, copy-as/clipboard and live parity cases, hook inertness checks).** `smbios_gui_ci_test` runs Dear ImGui frames with no window,
no GL and no OS hooks on the harness `tests/gui/support/headless_imgui.{hpp,cpp}`
(`HeadlessImGuiContext`: in-memory clipboard, refusing `OpenInShell`, no IME
hook, no `imgui.ini`/log file, a null renderer that acknowledges 1.92 texture
requests). Every SMBios is replaced through `load_table()` before any frame,
and every assertion on captured text or clipboard bytes is the boolean,
counts-only form - captured or fixture text is never printed (threat model
`docs/security/2026-11-11-gui-test-tiers.md`, R-5). The suite passes with
`--random=1`. CI's `gui-build` job runs it on all three OSes
(`ctest -L gui-ci --timeout 60`). It uses `imgui_internal.h` (symbol list in
`headless_imgui.hpp`), so a vcpkg baseline bump that changes the imgui version
must re-run it.

The local tier (`smbios_gui_local_test`, 8 cases, one CTest entry each, plus
the 2 `smbios_gui_autoexit.*` inertness checks) runs on a developer machine
only and every case skips as root. Its live-data rules (threat model R-18 to
R-20): rendered entries, CLI stdout and clipboard text stay in memory and
are compared only through `same_bytes()`/`find_bytes()`
(`tests/gui/support/live_table_support.{hpp,cpp}`, which never print their
operands); output is counts, node indices, type codes and handles only;
parity-style failures print one `<what>: expected -> actual` line per
difference (CLI = expected, GUI = actual; at most 20 plus a summary); a
`SKIP:` line is only ever printed before the first assertion, so it cannot
mask a failure. `tests/gui/support/glfw_test_window.{hpp,cpp}`
(`GlfwTestWindow`) is the RAII hidden window (+ ImGui context and GLFW /
OpenGL3 backends, `glfwSwapInterval(0)`, no `imgui.ini`, GLFW errors counted
not printed) that records each teardown call. The two Task 01 placeholder
cases were removed: every real case proves the tier builds and runs, and the
skip path was re-proven by forcing a display-probe and a no-table skip (both
reported *Skipped*, then reverted); no case skips on the maintainer's Mac, so
the skip path is not exercised on every run there. **Clipboard and platform
caveats:** `ClipboardRoundTripMatchesCli` writes the real OS clipboard 4
times (one identifier-free entry, one write per format) and restores the
user's text only if the clipboard still holds its last write; non-text
content is lost, a crash/timeout/Ctrl-C leaves the BIOS Information text on
the clipboard, X11/Wayland drops the restored text when the test exits
unless a clipboard manager keeps it, and clipboard history/sync tools keep
the 4 writes ([manual-testing.md](manual-testing.md), *Clipboard caveats*).
The tier has only run on macOS; its Windows branches are unverified (see
the child-process paragraph above) and the Linux run is pending.

The Task 03 suites compile the real `smbios_gui/src/entry_tree_view.cpp` and
`copy_as_action.cpp` into `smbios_gui_ci_test`; a configure-time check in
`tests/gui/CMakeLists.txt` fails the configure if either is ever added to
`libsmbios_gui_model`'s sources (threat model R-10). Their shared fixture
`tests/gui/support/browser_fixture.{hpp,cpp}` (`BrowserFixture`,
`render_browser()`) wraps *every* `render_entry_browser()` call in the S3
stack-balance check. Two spike limits shape them: **S4** - captured text
holds every row (logging turns clipping off), so text is only used for
things that must be *present*; the two clipper cases instead hit-test in
normal frames (move the mouse down the pane, read Dear ImGui's `HoveredId`
and map it back to a row index), which proves which rows are really
submitted where, but not pixels - visual layout stays manual (M-07).
**S6** - the duplicate-ID signal is hover-scoped, so
`DuplicateHandlesNoIdConflict` reads it while each duplicate row is hovered
(both must be hovered for the check to count); a whole-frame check would
need imgui built with `IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS`.
`copy_as_action.cpp`'s function-local `static CopyAsState` outlives each
case: `GuiCopyAsHeadlessTests` keeps the remembered format at `json`
between cases (a RAII restorer re-selects it through the combo, also on
failure), and cases that click Copy select `json` first. Without the
restorer, about one random order in three fails; with it, the suite passes
with `--random=1`.

Group-level totals: `smbios_gui_ci_test` has 25 cases in 3 suites (6 + 13 + 6); `smbios_gui_local_test` has 8 cases in the one suite `SmbiosGuiLocalTests`, which with the 2 `smbios_gui_autoexit.*` checks gives 10 local CTest entries. Pixels and visual layout, the root warning (M-12) and window behaviour (M-13) are not covered by any tier.

| Suite | Covers | Guards against |
|-------|--------|------------------|
| `GuiHeadlessSpikeTests` (`ci/headless_spike_test.cpp`, 6 cases: `S1HeadlessFrameLoopRuns1000Frames`, `S2NoOsSideEffects`, `S3BalancedStacksAroundRenderEntryBrowser`, `S4TextCaptureSeesRenderedText`, `S5LocatorActivatesWidgetsWithoutPixels`, `S6IdConflictSignalIsHoverScoped`) | S1: 1000 frames on the null renderer, no pending texture request, at most 2 tracked textures. S2: every OS hook replaced and unchanged after every frame, the clipboard kept in memory, `OpenInShell` refused, no `imgui.ini`/`imgui_log.txt` in the CTest working directory after 6.7 s of simulated frames. S3: equal `ErrorRecoveryStoreState()` snapshots around the real `render_entry_browser()` for every node and the empty table, plus a negative control. S4: `LogToBuffer()` captures labels, field rows and widget text; logging disables clipping, so all 200 rows of a 200-node table are captured. S5: `ActivateItemByID()` with IDs computed from the widget's ID path (no pixel coordinates): a node click, the Hex toggle, Copy into the in-memory clipboard (byte-equal to `render_entry_as()` in exactly one format), opening the format combo. S6: the only duplicate-ID signal (`ImGuiContext::DebugDrawIdConflictsId`) is hover-scoped | The CI tier touching the runner's clipboard, shell, IME or disk; texture requests piling up with no renderer; `render_entry_browser()` leaving an ID/style/window stack unbalanced; the text, locator or ID-conflict oracles that Task 03 builds on silently stopping working (for example after an imgui version bump) |
| `GuiEntryBrowserHeadlessTests` (`ci/entry_browser_headless_test.cpp`, 13 cases: `EmptyNodesShowsNoTableText`, `SelectionIndexClampedEveryOutOfRangeValue`, `RegisteredAndUnknownNodesRender`, `FieldRowsMatchFieldValueText`, `DecodeFailedNodeShowsMarker`, `HostileStringsRenderAsPlainText`, `DuplicateHandlesNoIdConflict`, `IdAndStyleStacksBalancedEveryPath`, `NodeListClipperShowsContiguousTail`, `FieldTableClipperAboveThreshold`, `HexToggleSwitchesNumericRendering`, `NodeClickChangesSelection`, `ThousandFramesStable`) | The real `render_entry_browser()`: the empty-table text only (no Copy/Hex/panes); selection clamping for -1, `INT_MIN`, size, size+1 and `INT_MAX`; every node's label and `Unknown (0xC8)` listed, and an unregistered node showing no table or Copy; every field row of the doc-sample fixture captured as `\| name \| field_value_text \|` in order, covering all four `FieldValue` alternatives; the decode-failed marker and explanation; hostile labels, field names and values (`%n`, `##x`, `###id`, C0/bidi controls, Actions `::` commands, 10 000 bytes) shown as `display_text()` verbatim, with `##`/`###` visible and selection by index unaffected; duplicate handles as two distinct, separately selectable IDs with the S6 signal 0 while each is hovered; balanced stacks on every path; 10 000 nodes and 5000-/200-row field tables reserving every row's height and hit-testing to one contiguous run from row 0 at the top and to the last row after `SetScrollY()` to the bottom, bounded by the pane height; Hex on/off switching uint64 rows between decimal and `field_value_text(v, true)`; node clicks changing the selection and the field pane; 1000 frames with constant captured length | A firmware string used as an ImGui label, ID or format string (G-1/G-2); handle-keyed IDs (fails 5 cases when tried); a clipper showing blank, wrong or missing rows (G-3; a skipped row or a row showing its neighbour's data fails the clipper cases); an out-of-range selection crashing; Copy offered for a node with no entry; the Hex toggle not applying; unbalanced ImGui stacks; per-frame accumulation |
| `GuiCopyAsHeadlessTests` (`ci/copy_as_headless_test.cpp`, 6 cases: `DefaultFormatIsJsonNoUnsafeNote`, `NonJsonFormatShowsUnsafeNote`, `ComboListsExactlyRegisteredNames`, `CopyClickPutsRenderEntryAsBytesOnClipboard`, `CopyRefusesOversizedOutput`, `NoCopyControlsForEmptyFieldNode`) | The real `render_copy_as_action()` inside the browser: the combo preview `json` with no unsafe-format note; `yaml` chosen through the combo shows the note, `json` again removes it; the open combo lists exactly `registered_output_formatter_names()` (sorted, no extras); Copy on every registered node puts exactly `render_entry_as(..., "json")` on the in-memory clipboard (one write per click) with `Copied to clipboard.`; output over `kCopyAsMaxBytes` (two 600 KiB OEM strings) is refused with no clipboard write and `Entry too large to copy`; unregistered and decode-failed nodes have no Copy text, no openable combo and no clipboard write | The clipboard diverging from the CLI's formatter output (a dropped first byte fails it); the cap weakened or bypassed; a terminal-unsafe format without its warning; the dropdown drifting from the formatter registry; Copy-as state leaking between cases or selections |
| `SmbiosGuiLocalTests` (`local/hidden_window_smoke_test.cpp`, 1 case: `HiddenWindowSmoke`). Type: integration (local-only). Needs: display + GL; live table | 300 frames of the real `render_entry_browser()` over the live table in a hidden `GlfwTestWindow` (same context hints as `main.cpp`, GLFW + OpenGL3 backends, real GL draw and swap), selecting node `frame % size` so every node's field pane is drawn; ImGui stacks the same size before/after the browser on every frame (Task 02 S3 oracle, `stack_sizes_equal()`); selection index left as set; 0 GLFW errors; teardown made all five shutdown calls in reverse init order and left no ImGui context current | A crash, GL error or unbalanced Begin/Push in the real backend stack over real firmware data (which synthetic tables cannot reproduce); a teardown step skipped or reordered |
| `SmbiosGuiLocalTests` (`local/clipboard_round_trip_test.cpp`, 2 cases: `CopyAsBytesMatchCliEveryNodeAndFormat`, `ClipboardRoundTripMatchesCli`). Type: integration (local-only). Needs: live table and the `smbios` CLI; the second also display + GL and the OS clipboard | `CopyAsBytesMatchCliEveryNodeAndFormat` (in memory, no clipboard): the formatter registry is exactly `json`/`text`/`xml`/`yaml`; for every registered node and each format, `render_entry_as()` equals the formatter's own `render()` byte for byte and occurs verbatim, in node order, in `smbios --format <name>`'s stdout (26 nodes x 4 formats on the maintainer's Mac). `ClipboardRoundTripMatchesCli`: one identifier-free entry (type 0 when present; field names checked for `serial`/`uuid`/`asset`, values never read) rendered in the 4 formats, each found in the CLI's output, then written with `glfwSetClipboardString()` and read back byte-identical; the user's clipboard text restored and read back identical (compare-before-restore guard; empty saved value restores to empty; notice line before the first write and after the restore) | The "Copy as..." bytes diverging from the CLI for any live entry or format (M-04/M-09/M-10); the OS clipboard (GLFW, UTF-8 conversion) altering bytes; the user's clipboard not restored, or overwritten after they changed it (negative proofs: a truncated restore fails `RestoreNotVerified`; a foreign write before restore is left alone and fails `ChangedByOthersNotRestored`). Skips (before any assertion): root, no display, no readable table, no identifier-free entry (accepted gap, documented) |
| `SmbiosGuiLocalTests` (`local/live_parity_test.cpp`, 1 case: `LiveParityRegisteredNodesMatchCli`). Type: integration (local-only). Needs: live table and the `smbios` CLI; no display | The GUI model's registered nodes (`SMBiosEntryFactory::create()` non-null, the ruled predicate) vs `smbios --format json` split in memory (string-aware; JSON because the text format can forge `Header type:` lines, T-9): count, per-position structure type, and each entry's JSON object bytes (handle order follows: `create_entry_for_node()` re-checks each node's type and handle at its structure position); prints `GUI-only unregistered nodes: N` once. Maintainer's Mac: 26 = 26 registered, 32 GUI nodes, extras line 6 | GUI/CLI drift in which structures are listed, their order, or their decoded content (M-02/M-03); a decode failure on real hardware. Negative proofs: swapping two same-type nodes gives two `json object bytes` lines with handles, swapping different types gives `registered node #0 type: 7 -> 19`. Skips only when both sides see 0 structures; any other disagreement fails |
| `SmbiosGuiLocalTests` (`local/launch_autoexit_twin_test.cpp`, 1 case: `LaunchAutoExitTwin`). Type: integration (local-only). Needs: display + GL; a window appears for about 2 s | The twin launched with `--exit-after-frames 120` in a fresh `mkdtemp` directory exits by itself with status 0, writes 0 bytes to stdout+stderr together, and leaves the directory empty | Stray console output or an `imgui.ini`/log file from a normal run (M-14); a launch or teardown regression (M-01). Differs from `AutoExitTwinSmoke` by these M-14 assertions |
| `SmbiosGuiLocalTests` (`local/autoexit_twin_smoke_test.cpp`, 2 cases: `AutoExitTwinSmoke`, `AutoExitTwinRejectsBadArguments`). Type: integration (local-only). Needs: display + GL for the smoke only; skips as root | `AutoExitTwinSmoke`: the twin launched through `spawn_and_capture()` with `--exit-after-frames 30` from a fresh `mkdtemp` directory exits by itself with status 0 before the 30 s deadline (a window briefly appears; skips with one `SKIP:` line when the hidden-window probe fails). `AutoExitTwinRejectsBadArguments` (no display needed - the hook parses argv before any window or table read): 15 malformed command lines (none, missing N, `0`, `100001`, 7 digits, `+5`, ` 5`, `5x`, `0x10`, empty, `-1`, wrong flag, `--exit-after-frames=5`, an extra argument, swapped order) each give exit status 2, empty stdout, exactly the fixed usage line on stderr, and leave the working directory empty | The hook accepting a malformed N or echoing argv; the twin failing to start, link or tear down cleanly, hanging, or exiting non-zero after N frames; the spawn helper not reaping or not enforcing its deadline. On failure, first check: that the twin was rebuilt (it, the shipped `smbios_gui` and the `smbios` CLI are build-order dependencies of `smbios_gui_local_test`), that a display session exists, and whether a macOS permission prompt held the window |
| `SmbiosGuiLocalTests` (`local/process_spawn_test.cpp`, 1 case: `SpawnDoesNotInheritDescriptors`). Type: integration/regression (local-only), POSIX only (a no-op pass on Windows). Needs: `/bin/ls`; no display, no table; skips as root | The test process opens a non-close-on-exec descriptor (number >= 100) and runs `/bin/ls /dev/fd` through `spawn_and_capture()`: exit 0, 0/1/2 each listed once, the marker never listed, at most two other entries (the directory handles `ls` itself opens: `3` and `4` on macOS) | A descriptor of the test process (GLFW/window-system connection, Boost.Test file, ...) leaking into the twin or the CLI (R-16); fails against a helper without the child's close loop (the marker and the test process's other descriptors are listed). On failure, first check: the close loop in `process_spawn.cpp`'s POSIX child branch and whether `/bin/ls` lists `/dev/fd` on this OS |
| `smbios_gui_autoexit.InertBinaries`, `smbios_gui_autoexit.InertCompileLines` (`support/hook_inertness_check.cmake`, run with `cmake -P`) | Hook strings 0 in the shipped `smbios_gui`, >= 1 in the twin (`grep -a -c -F -e`); `SMBIOS_GUI_TEST_HOOKS` 0 on `ninja -t commands smbios_gui`, >= 1 on the twin's, and 0 on smbios_gui's `compile_commands.json` lines if that file exists. Fails closed on a missing binary or tool; prints counts only | The test hook leaking into the shipped binary or its compile line (threat model T-4/R-15). CI's `gui-build` job repeats the shipped-binary half on all three OSes |

### Mock - `tests/mock/` (`smbios_mock_test`)

**Intentional placeholder.** `SmbiosMockTests::NoMockableSeamYetTestCase`
exists only to document *why* this suite has no real cases yet and to keep
Boost.Test from treating a zero-case suite as a setup error - it makes no
assertion about production behavior. `SMBios` and `PhysicalMemory` both
select their platform source via a compile-time `#if`, not an injectable
interface, so there is no boundary to substitute a test double at. This
closes the moment milestone 0005's `ISmbiosSource` interface lands - see
**Intended coverage** below.

### Perf - `tests/perf/` (`smbios_perf_test`)

`SmbiosPerformanceTests::PerformanceTestsCase` times a real `SMBios`
construction and full-table enumeration against the host's live firmware
table, logging per-structure timing via `BOOST_TEST_MESSAGE`. Needs
root/admin on most platforms (the same privilege the native source itself
needs) and fails cleanly, not catastrophically, without it. Guards against
a parsing-path regression that is correct but silently pathological (e.g.
an accidental O(n²) re-scan per structure) - a defect class unit tests
cannot see since they never run against a realistically-sized real table.

### Integration - `tests/integration/` (`smbios_integration_test`)

`SmbiosFunctionalTests` constructs a real `SMBios` against the live
machine's actual firmware table (`SMBiosCreationTestCase`:
version/structure-count/table-base/table-size are all non-empty;
`SMBiosIterationTestCase`: every yielded header is non-empty) and factory-
creates a real entry object per header. Needs root/admin and fails cleanly
without it, same as Perf. This is the suite that proves the whole stack -
native source discovery, legacy-scan fallback, table walk, entry
factory - actually produces a sane result end-to-end on a real host,
which no synthetic-buffer unit test can claim regardless of how many
tiers it covers.

CTest-level smoke tests are also classified **Integration** (they run the
built `smbios` binary or an example, not a Boost.Test case):

- `smbios_cli_*` (`tests/integration/CMakeLists.txt`):
  `smbios_cli_unknown_format` (`--format bogus` must exit non-zero),
  `smbios_cli_unknown_format_lists_names` (stderr lists the registered
  formats), and `smbios_cli_format_csv` (`--format csv` header line; reads
  real hardware, so it is the only one that can fail without privileges).
- `smbios_example_*` (`examples/CMakeLists.txt`): the `format_table`
  example for `text`/`xml`/`json`/`yaml`, its unknown-format case,
  `read_fields`, and (when Python 3 is found) the `*_parses` structural
  checks. They degrade by design (no readable table gives a notice, an empty
  document and exit 0), so they are must-pass even without privileges.

### C ABI - `tests/c_abi/` (five plain executables, not Boost.Test)

Each proves one slice of the `smbios_c` shim (milestone 0007) against
the real machine's SMBIOS table: `smbios_c_compile_as_c_test` (the
public header parses under a real C compiler, not just C++ with
`extern "C"` ignored), `smbios_c_example` (a genuine end-to-end C
program - open, iterate, render, close), `smbios_c_open_close_test`
(handle lifecycle and the `SMBIOS_ERR_NO_TABLE_FOUND` path),
`smbios_c_entry_iteration_test` and `smbios_c_typed_getters_test` (every
entry's rendered description, and the five curated typed getters, cross-
checked byte-for-byte against the equivalent direct C++
`SMBiosEntryFactory::create()`/`render_to_description()` calls). The
latter two link `libsmbios` directly (for that cross-check) in addition
to `smbios_c`, and need the same whole-archive link treatment
`smbios_c`'s own `CMakeLists.txt` uses - see that file's comment for why.
Guards against the exact gap discovered while first verifying this
milestone by hand: a binary linking `libsmbios` as a plain static
archive without whole-archive silently drops entry-type registration for
any type never directly named in that binary's own code, so `smbios_c`'s
behavior could silently diverge from the CLI's for exactly those types
without a test exercising every real structure, not just the ones the
shim's own source happens to name.

### CI scripts - `scripts/ci/tests/` (`python3 scripts/ci/run_tests.py`, Python `unittest`)

Milestone 0014 Story 01.0 Task 03. The fixtures in `scripts/ci/tests/fixtures/` are small synthetic logs with no firmware data. The 50 MB `huge.log` is generated at test time and never committed.

| Suite (file) | Cases | Covers | Guards against |
|---|---|---|---|
| `ReportContractTests` (`test_ci_step.py`) | 17 | RC1 grammar and ceilings (one `ok:` line; at most 20 diagnostic + 40 tail lines, 64 lines and 16 KiB; tail never repeats a diagnostic); a 50 MB log with peak traced memory under 64 MiB; a hostile log fenced by `stop-commands` with a fresh per-call token and an escaped annotation; RC5 best-effort exits 0 with an `outcome=warn` record; exit 127/143/70 and usage exit 2 with no traceback; both ctest summary wordings; the pattern sets against the cxx/boost/ASan fixtures; `ACTIONS_*_DEBUG` not passed to the child; `self-test`; `--help` under `-I` | CI output floods, injected workflow commands, lost exit codes, and summary-text parsing that breaks on a ctest version change (plan E4) |
| `ReporterBackendTests` (`test_ci_step.py`) | 8 | ADR 0005: `github`, `teamcity` and `plain` each render every neutral event (`step_ok/fail/warn/skip`, the untrusted-block fence, `internal_error`, `job_summary`). Each backend is checked against a hostile `::error::` line and a hostile `##teamcity[` line. Also covers the `%` and `\|` escaping and backend selection through `--reporter`/`CI_REPORTER` | A backend that leaks a service message or workflow command out of the fence, or drifts from the ADR 0005 mapping table |
| `EnvNamesTests` (`test_ci_step.py`) | 5 | Neutral-name accessor: a missing required name exits 2 with one line; local defaults apply; unknown or platform names are rejected | Scripts that fail obscurely, or read platform variables, when the CI adapter forgets a name |
| `P11PolicyTests` (`test_ci_step.py`) | 2 | No `${{`, `%teamcity.`, `##teamcity[`, `::error`, `::warning`, `::stop-commands` or `GITHUB_`/`RUNNER_`/`TEAMCITY_` names under `scripts/ci/`. Exempt: `reporters/`, `env_names.py` and `tests/`. Includes a positive control for the scanner | Platform syntax leaking into neutral scripts (ADR 0005 portability) |
| `ShellFlagsHarness` (`test_shell_flags.py`) | 3 | Runs shell under `bash --noprofile --norc -e -o pipefail` with Python stubs on a temporary `PATH`. The 0011 `run_quiet` bug form fails silently; the corrected one-call form exits with the documented code after printing its RC1 lines | Plan E5: shell that was never tested under the runner's flags |
| `ShellScriptsUnderRunnerFlags` (`test_shell_flags.py`) | 2 | Every `scripts/ci/**/*.sh` (declares `# ci-expected-exit: <n>`), and workflow `run:` blocks. Both skip today: there is no `*.sh`, and the extractor is not yet built (0014 Story 02.0 Task 03) | E5 regressions in any shell added later |

## Intended coverage (committed, not yet implemented)

Each item below is a specific roadmap commitment, not a vague aspiration -
linked to the milestone/story that owns it.

- **Milestone 0003, Story 03.0, Task 02** - bounds-checked string-section
  parsing tests (`AbstractSMBiosEntry::parse_dmi_strings()` against a table
  missing its terminating double-null marker), the string-side counterpart
  to the table-walk tests already landed above.
- **Milestone 0004** - a unit suite per newly-implemented SMBIOS structure
  type (the factory's expanded coverage beyond today's three entry
  classes), following the exact length-tier pattern `BiosInformationEntry`/
  `MemoryDeviceEntry` already established.
- **Milestone 0005** - the real `SmbiosMockTests` suite: once `SMBios`
  depends on the `ISmbiosSource` interface instead of a compile-time `#if`,
  this is where a fake source gets substituted to assert chain/fallback
  behavior (e.g. "falls through to the next source when `is_available()`
  is false") without needing real hardware or root. Story 01.1 (per-OS
  source libraries) lands the injectable seam's library boundary itself
  - two new unit tests proving `smbios_legacy` links standalone and
  that `smbios::make_default_source_chain()` resolves through `smbios`
  core's link graph - ahead of any real fake `ISmbiosSource`; the mock
  placeholder's remaining blocker after that story is a concrete fake
  source, not a missing architectural seam.
- **Milestone 0006, Story 01.0** - a hermetic `SyntheticTableBuilder`
  (in-memory, well-formed SMBIOS table construction bypassing every
  `ISmbiosSource`), with every ad hoc synthetic buffer this document's
  existing unit suites hand-roll today migrated onto it, plus a CI job
  running the full hermetic suite unprivileged on every push - closing the
  gap where Perf/Integration can only prove the code *builds*, not that it
  *behaves correctly*, on a CI runner.
- **Milestone 0006, Story 02.0** - dedicated boundary/malformed-table
  tests beyond what Story 03.0 above covers: a structure count that
  disagrees with how many structures are actually found before
  `EndOfTable`, and a documented, locally-runnable fuzz harness feeding
  arbitrary bytes into `read_smbios_table()` (not run continuously in CI -
  a non-goal for that milestone).
