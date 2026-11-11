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

**GUI (`smbios_gui`, milestone 0011).** Its ImGui-free view-model layer (`libsmbios_gui_model`) is unit-tested in `tests/unit/gui_*_test.cpp`; the rendering code (`render_entry_browser()`, `render_copy_as_action()`) is also exercised headlessly by the CI-safe tier below; the window, the OS clipboard and visual layout are covered by the manual plan in [manual-testing.md](manual-testing.md). Story 04.0 adds two GUI test tiers under `tests/gui/`, both only configured when `SMBIOS_BUILD_GUI=ON` (a non-GUI build adds no target and no test). Run them with `ctest -L gui-ci`, `ctest -L gui-local` or `ctest -L gui`:

| Category | Directory | Target / CTest name(s) | CTest labels | Needs root/admin? | Always expected green? |
|----------|-----------|--------------------------|----------------|--------------------|--------------------------|
| Unit (GUI, CI-safe) | `tests/gui/ci/` | `smbios_gui_ci_test` (one CTest entry) | `gui;gui-ci` | No (no window, GL or OS clipboard; synthetic tables only) | Yes - never skips; runs in CI's `gui-build` job on all three OSes |
| Integration (GUI, local-only) | `tests/gui/local/` | `smbios_gui_local_test`, one CTest entry per case: `smbios_gui_local_test.<CaseName>` | `gui;gui-local` | No | Yes, or *Skipped* when a display/GL/clipboard/table is missing - never on CI |

The local-only tier is controlled by `SMBIOS_GUI_LOCAL_TESTS` (default ON for a GUI build outside CI). When the `GITHUB_ACTIONS` or `CI` environment variable is set at configure time it is forced OFF with a warning, and `tests/gui/CMakeLists.txt` re-checks the CI environment where the targets are created, so under CI the tier is not configured, compiled or registered. CI's `gui-build` job proves it, failing closed: `ctest -N -L gui-local` and `ctest -N -R 'smbios_gui_local_test|smbios_gui_autoexit'` must both print `Total Tests: 0` (a missing line is a failure), and no `smbios_gui_local_test*`/`smbios_gui_autoexit*` file or directory may exist in the build tree. The same job checks that the shipped `smbios_gui` contains the test-hook string `--exit-after-frames` 0 times (`grep -a -c -F -e`). Every local-only source also fails to compile unless the target-private definition `SMBIOS_GUI_LOCAL_ONLY_TESTS` is set (`tests/gui/support/local_only_guard.hpp`). A missing resource makes a local case print one `SKIP: <CaseName>: <reason>` line (`skip_unless()`, `tests/gui/support/local_skip.hpp`), which the CTest property `SKIP_REGULAR_EXPRESSION "(^|\n)SKIP: "` reports as *Skipped*. (`"^SKIP: "` does not work: CMake anchors `^` to the start of the whole output, which begins with Boost.Test's `Running 1 test case...` line.)

**No `e2e` category.** A hardware-inventory library/CLI has no user journey
distinct from "construct an `SMBios`, read its structures" - that journey
*is* the integration suite. Adding a separate `e2e` layer here would just
duplicate it under a different name.

Framework is Boost.Test (`BOOST_AUTO_TEST_SUITE`/`BOOST_AUTO_TEST_CASE`)
throughout, matching the vcpkg-declared `boost-test` dependency. Run one
suite directly for a focused check: `./smbios_unit_test
--run_test=<SuiteName> --log_level=message`; run everything via `ctest
--test-dir build/<preset> --output-on-failure`.

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

### GUI tiers - `tests/gui/` (`smbios_gui_ci_test`, `smbios_gui_local_test`)

**CI-safe tier: headless spike (Story 04.0, Task 02) and widget tests
(Task 03); local tier: Task 01 placeholders.** `smbios_gui_ci_test` runs Dear ImGui frames with no window,
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
must re-run it. The local tier is still the Task 01 placeholders; Task 04
replaces them.

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

| Suite | Covers | Guards against |
|-------|--------|------------------|
| `GuiHeadlessSpikeTests` (`ci/headless_spike_test.cpp`, 6 cases: `S1HeadlessFrameLoopRuns1000Frames`, `S2NoOsSideEffects`, `S3BalancedStacksAroundRenderEntryBrowser`, `S4TextCaptureSeesRenderedText`, `S5LocatorActivatesWidgetsWithoutPixels`, `S6IdConflictSignalIsHoverScoped`) | S1: 1000 frames on the null renderer, no pending texture request, at most 2 tracked textures. S2: every OS hook replaced and unchanged after every frame, the clipboard kept in memory, `OpenInShell` refused, no `imgui.ini`/`imgui_log.txt` in the CTest working directory after 6.7 s of simulated frames. S3: equal `ErrorRecoveryStoreState()` snapshots around the real `render_entry_browser()` for every node and the empty table, plus a negative control. S4: `LogToBuffer()` captures labels, field rows and widget text; logging disables clipping, so all 200 rows of a 200-node table are captured. S5: `ActivateItemByID()` with IDs computed from the widget's ID path (no pixel coordinates): a node click, the Hex toggle, Copy into the in-memory clipboard (byte-equal to `render_entry_as()` in exactly one format), opening the format combo. S6: the only duplicate-ID signal (`ImGuiContext::DebugDrawIdConflictsId`) is hover-scoped | The CI tier touching the runner's clipboard, shell, IME or disk; texture requests piling up with no renderer; `render_entry_browser()` leaving an ID/style/window stack unbalanced; the text, locator or ID-conflict oracles that Task 03 builds on silently stopping working (for example after an imgui version bump) |
| `GuiEntryBrowserHeadlessTests` (`ci/entry_browser_headless_test.cpp`, 13 cases: `EmptyNodesShowsNoTableText`, `SelectionIndexClampedEveryOutOfRangeValue`, `RegisteredAndUnknownNodesRender`, `FieldRowsMatchFieldValueText`, `DecodeFailedNodeShowsMarker`, `HostileStringsRenderAsPlainText`, `DuplicateHandlesNoIdConflict`, `IdAndStyleStacksBalancedEveryPath`, `NodeListClipperShowsContiguousTail`, `FieldTableClipperAboveThreshold`, `HexToggleSwitchesNumericRendering`, `NodeClickChangesSelection`, `ThousandFramesStable`) | The real `render_entry_browser()`: the empty-table text only (no Copy/Hex/panes); selection clamping for -1, `INT_MIN`, size, size+1 and `INT_MAX`; every node's label and `Unknown (0xC8)` listed, and an unregistered node showing no table or Copy; every field row of the doc-sample fixture captured as `\| name \| field_value_text \|` in order, covering all four `FieldValue` alternatives; the decode-failed marker and explanation; hostile labels, field names and values (`%n`, `##x`, `###id`, C0/bidi controls, Actions `::` commands, 10 000 bytes) shown as `display_text()` verbatim, with `##`/`###` visible and selection by index unaffected; duplicate handles as two distinct, separately selectable IDs with the S6 signal 0 while each is hovered; balanced stacks on every path; 10 000 nodes and 5000-/200-row field tables reserving every row's height and hit-testing to one contiguous run from row 0 at the top and to the last row after `SetScrollY()` to the bottom, bounded by the pane height; Hex on/off switching uint64 rows between decimal and `field_value_text(v, true)`; node clicks changing the selection and the field pane; 1000 frames with constant captured length | A firmware string used as an ImGui label, ID or format string (G-1/G-2); handle-keyed IDs (fails 5 cases when tried); a clipper showing blank, wrong or missing rows (G-3; a skipped row or a row showing its neighbour's data fails the clipper cases); an out-of-range selection crashing; Copy offered for a node with no entry; the Hex toggle not applying; unbalanced ImGui stacks; per-frame accumulation |
| `GuiCopyAsHeadlessTests` (`ci/copy_as_headless_test.cpp`, 6 cases: `DefaultFormatIsJsonNoUnsafeNote`, `NonJsonFormatShowsUnsafeNote`, `ComboListsExactlyRegisteredNames`, `CopyClickPutsRenderEntryAsBytesOnClipboard`, `CopyRefusesOversizedOutput`, `NoCopyControlsForEmptyFieldNode`) | The real `render_copy_as_action()` inside the browser: the combo preview `json` with no unsafe-format note; `yaml` chosen through the combo shows the note, `json` again removes it; the open combo lists exactly `registered_output_formatter_names()` (sorted, no extras); Copy on every registered node puts exactly `render_entry_as(..., "json")` on the in-memory clipboard (one write per click) with `Copied to clipboard.`; output over `kCopyAsMaxBytes` (two 600 KiB OEM strings) is refused with no clipboard write and `Entry too large to copy`; unregistered and decode-failed nodes have no Copy text, no openable combo and no clipboard write | The clipboard diverging from the CLI's formatter output (a dropped first byte fails it); the cap weakened or bypassed; a terminal-unsafe format without its warning; the dropdown drifting from the formatter registry; Copy-as state leaking between cases or selections |
| `SmbiosGuiLocalTests` (`local/gui_local_main.cpp`, 2 cases: `ScaffoldingPlaceholder`, `SkipHelperPlaceholder`) | Placeholder: the local tier builds and runs; `SkipHelperPlaceholder` always skips through `skip_unless()` so CTest reports it *Skipped* while its passing sibling stays *Passed* | A missing-resource skip being reported as Passed or Failed (the `^SKIP: ` regex bug) |

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
