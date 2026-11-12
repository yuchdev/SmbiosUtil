# 03 - CI-safe tier: headless widget tests

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-12); verified on Linux, macOS and Windows runners (CI run 43074117927, 8/8 green) (case list subject to Task 02's spike outcome)

## Objective

Use Task 02's `HeadlessImGuiContext` to cover the Dear ImGui rendering code (`render_entry_browser()`, `render_copy_as_action()`) that is currently verified only by manual run. Cover its structural and safety properties, not pixels. All of it runs on every push in `gui-build` on three OSes.

## Files

- **Create** `tests/gui/ci/entry_browser_headless_test.cpp` (`BOOST_AUTO_TEST_SUITE(GuiEntryBrowserHeadlessTests)`).
- **Create** `tests/gui/ci/copy_as_headless_test.cpp` (`BOOST_AUTO_TEST_SUITE(GuiCopyAsHeadlessTests)`).
- Fixtures: `SyntheticTableBuilder` tables plus `gui_test_support.hpp` helpers. Where a case needs a node shape that no table can produce (for example `decode_failed`, or 10 000 nodes), it uses hand-built `std::vector<EntryNode>`. That is legal because `render_entry_browser()` takes `nodes` as its only display input. In that case `bios` is a synthetic or empty `SMBios`.

## Cases

The oracle names below refer to Task 02's spike items. A case whose oracle failed in the spike is dropped and recorded.

**`GuiEntryBrowserHeadlessTests`**

> **Amended after the Task 02 spike (2026-11-11, finding S4):** while ImGui logs (`LogToBuffer`), it turns clipping off, so the captured text contains **every** row, including rows a real frame would clip away (a 200-row table captured all 200). The text oracle therefore **cannot** prove visibility. Consequences for the cases below: `NodeListClipperShowsContiguousTail` and `FieldTableClipperAboveThreshold` must not assert "a contiguous visible range / bounded visible-row count" from captured text. Use another oracle - e.g. the draw data of a **non-logging** frame (clipped rows emit no draw commands/vertices, so compare vertex/command counts for a short versus a long list; and scroll range from the child window's content size and max scroll) - and if no reliable oracle exists, say so, keep only the balanced-stack and no-crash assertions in `gui-ci`, and leave the visibility check to the manual plan (M-07). Cases that only need text to be *present* (`HostileStringsRenderAsPlainText`, `RegisteredAndUnknownNodesRender`, `FieldRowsMatchFieldValueText`, ...) are unaffected. Also (finding S5, requirement R-10): `copy_as_action.cpp` keeps a function-local `static CopyAsState`, which S5's Copy click leaves at "Copied" for node 0; every `GuiCopyAsHeadlessTests` case must reset it or not depend on its initial value, and the suite must pass with `--random=1`.

| Case | Asserts | Oracle |
|------|---------|--------|
| `EmptyNodesShowsNoTableText` | `nodes = {}`: the captured text contains `kNoTableText`, and contains no `Copy` and no `Hex`. `selected_index` ends at 0. Stacks are balanced. | S3, S4 |
| `SelectionIndexClampedEveryOutOfRangeValue` | For each of `-1`, `INT_MIN`, `nodes.size()`, `INT_MAX`: `selected_index == 0` after one frame. A valid index is left unchanged. | none (direct) |
| `RegisteredAndUnknownNodesRender` | A synthetic table with registered types and one OEM type: every visible node's `entry_node_label()` occurs in the captured text, and the `Unknown (0xNN)` node is present. Selecting the unknown node (S5) shows no field rows and no Copy controls. | S4 (S5) |
| `FieldRowsMatchFieldValueText` | For the selected registered node, each visible row's name and `field_value_text(value, hex=false)` occur in order in the captured text. Uses the doc-sample fixture, so all four `FieldValue` alternatives appear (M-04, M-05). | S4 |
| `DecodeFailedNodeShowsMarker` | A hand-built node with `decode_failed = true`: `kDecodeFailedText` is in the captured text, the fixed explanation is shown instead of a table, and stacks are balanced. | S3, S4 |
| `HostileStringsRenderAsPlainText` | Labels and field strings include `%n%s%x`, `%%`, `##x`, `###id`, embedded C0 controls, bidi controls and a 10 000-byte string. The captured text contains `display_text(s)` verbatim for each. In particular the `##x` and `###id` suffixes are **visible**, which proves the strings were never used as a label or ID (G-1/G-2). There is no crash, and stacks are balanced. | S3, S4 |
| `DuplicateHandlesNoIdConflict` | Two nodes with the same type and handle: both are rendered and selectable by index, stacks are balanced, and S6's conflict signal is zero (if S6 exists). | S3, S5, S6 |
| `IdAndStyleStacksBalancedEveryPath` | The snapshot comparison runs around every `render_entry_browser()` call in this suite, factored into a fixture helper. It is listed here as one explicit case over empty, normal, decode-failed and hostile inputs. | S3 |
| `NodeListClipperShowsContiguousTail` | 10 000 hand-built nodes. After scrolling `##entry_list` to the bottom (internal `SetScrollY` on the child window), the captured labels are a contiguous index range that includes the last node, and their number is bounded by the visible-row count. That means no blank or wrong rows (G-3, M-07). | S4 |
| `FieldTableClipperAboveThreshold` | One node with 5000 rows (more than `kFieldClipperThreshold`) and one with 200 rows (fewer). Both render with balanced stacks. The 5000-row table shows a contiguous visible row range at the top and, after scrolling, at the bottom. | S3, S4 |
| `HexToggleSwitchesNumericRendering` | Activate `Hex` (S5): numeric rows switch to `field_value_text(v, true)`. Activate it again and they return to decimal. Leaves the state as it found it (M-06). | S4, S5 |
| `NodeClickChangesSelection` | Activate node *k*'s `##node`: `selected_index == k`, and the field pane shows node *k*'s rows. | S4, S5 |
| `ThousandFramesStable` | 1000 frames over a normal table, with no growth in captured-text length between frames *n* and *n+1* (no per-frame accumulation) and balanced stacks. | S1, S3 |

**`GuiCopyAsHeadlessTests`**

| Case | Asserts | Oracle |
|------|---------|--------|
| `DefaultFormatIsJsonNoUnsafeNote` | The combo preview text `json` is captured, and `kCopyAsUnsafeFormatNote` is absent (M-08). | S4 |
| `NonJsonFormatShowsUnsafeNote` | Select `yaml` through the combo (S5): the note is present. Restore `json`. | S4, S5 |
| `ComboListsExactlyRegisteredNames` | With the combo open, every name in `registered_output_formatter_names()` is captured, sorted, with no extras. The unit registry contains `csv` and fakes, but this executable does not link them, so assert against the function rather than a literal list (M-08). | S4, S5 |
| `CopyClickPutsRenderEntryAsBytesOnClipboard` | Activate `Copy` for each registered node in a synthetic table: the captured clipboard bytes equal `*render_entry_as(*create_entry_for_node(...), "json")` byte for byte, and `Copied to clipboard.` is captured (M-09 click path, without the OS). | S2, S4, S5 |
| `CopyRefusesOversizedOutput` | An entry whose rendered output exceeds `kCopyAsMaxBytes` (OEM Strings with many maximal strings): the clipboard is unchanged and `Entry too large to copy` is captured. If no synthetic entry can exceed 1 MiB, record that and drop the case. Do not weaken the cap. | S2, S4, S5 |
| `NoCopyControlsForEmptyFieldNode` | Unknown and decode-failed nodes: no `Copy` and no `##format` text is captured. | S4 |

## Functions and Classes

None new beyond Task 02's harness. Fixture helpers stay file-local in an anonymous namespace, or in `tests/gui/support/` if two files share them, with Doxygen.

## Constraints

- **Order independence.** `copy_as_action.cpp` and `entry_tree_view.cpp` keep UI state in function-local `static`s (the remembered format and the Hex flag), and those persist across cases in one process. Every case that changes them restores them. The suite must pass both in default order and with `--random=1`.
- **No production change.** If a case seems to need a seam in `entry_tree_view.cpp` or `copy_as_action.cpp`, stop and escalate. Do not add one. The spike's internal-API locator exists precisely to avoid one.
- **Never print captured text or clipboard bytes.** This holds even though the data is synthetic, because a failure message must not dump hostile control bytes into the CI log. Report indices, counts and the case's own literal names.
- **Never skips.** No `precondition` decorator in `ci/`.
- `-Werror` clean on all three compilers.

## Tests

This task's cases are the tests. Run them locally with `ctest --test-dir build/macos-gui -L gui-ci`, and with `./smbios_gui_ci_test --random=1`. They also run on CI through Task 02's wiring.

## Success criteria

- [ ] Every case retained after the spike passes on macOS locally and in `gui-build` on all three OSes, including with `--random=1` (run locally).
- [ ] Each dropped case is listed in `status.md` with the spike item that failed, and with where its coverage now lives (local tier or manual).
- [ ] The diffs for `smbios_gui/src/` and `smbios_gui/include/` are empty for this task.
- [ ] Optional, local only: the suite is ASan/UBSan clean under the CI sanitizer flags on macOS. Record it if run.

## As-built notes (2026-11-12)

- **Delivered:** 19 cases in two suites (`GuiEntryBrowserHeadlessTests` 13, `GuiCopyAsHeadlessTests` 6), plus `tests/gui/support/browser_fixture.{hpp,cpp}` (wraps every `render_entry_browser()` call in the stack-balance check) and the configure-time R-10 check in `tests/gui/CMakeLists.txt` (fails if `entry_tree_view.cpp`/`copy_as_action.cpp` appears in the model library's sources or the target's own sources, and fails if it cannot read the lists). With the 6 spike cases, `smbios_gui_ci_test` has 25 cases. `smbios_gui/` and `.github/` are untouched. Assertions print only fixed text and counts (independent grep by the verifier: no `BOOST_CHECK*`/`cout`/`printf`; string/byte comparisons parenthesised so Boost cannot print operands).
- **S4 handled with a hit-testing oracle, not text capture or vertex counts:** the two clipper cases render normal (clipping-on) frames, move the mouse down the pane in half-row steps and map ImGui's `HoveredId` to row indices; they assert the content height reserves every row, that before scrolling the hovered rows form one contiguous run starting at row 0, after `SetScrollY` to the bottom one contiguous run ending at the last row, and that the run length is bounded both ways (measured on macOS: 41 of 10 000 list rows; 27-28 table rows of 5000 and 200). The spec's suggested draw-data vertex counts were rejected because ImGui culls off-screen text even without a clipper, so counts cannot tell a correct clipper from a broken one. **Limits, stated honestly:** this proves which rows exist and are hit-testable and where; it does **not** prove a row's drawn text matches its data (hover IDs are index-scoped; row content is covered by `FieldRowsMatchFieldValueText` with logging) and not pixels - visual layout stays manual (`docs/test/manual-testing.md` M-07). The field-table clipper cases use hand-built bool rows (their disabled `##value` checkbox is the only per-row item with an ID).
- **S6:** the duplicate-ID signal is hover-scoped, so `DuplicateHandlesNoIdConflict` reads it while each duplicate row is hovered (both must be hovered for the check to count) and documents the limit.
- **Order independence (R-10):** a fresh `HeadlessImGuiContext` per case and an RAII json-format restorer for the function-local Copy state; 55 of 55 `--random=1` runs on macOS, ASan/UBSan clean.
- **Deviations:** `DefaultFormatIsJsonNoUnsafeNote` cannot observe the true first-ever default under random order, so it checks that the remembered format is `json` between cases and that `default_copy_format_index(names)` names `json`; extra hostile inputs (Actions `::` commands) and a size+1 clamp case were added; the Task 02 harness cascade (an ImGui assert aborting one case makes later cases fail with "a Dear ImGui context is already current") was left unchanged - it only makes diagnosis noisier after a first failure.
- **Evidence:** CI run 43074117927 (8/8 jobs green): `gui-build` on `ubuntu-24.04`, `macos-latest` and `windows-latest` each printed `ok: ctest -L gui-ci (1 tests passed)` (one CTest entry for the whole binary), `ok: no test hook strings in the shipped smbios_gui` and `ok: no gui-local tests configured`.
- **Negative proofs (macOS, the shipped GUI code temporarily edited then restored; coder-reported):** a skipped tail row fails both clipper cases; dropping the first clipboard byte fails the Copy-click case for every node; keying node IDs by handle fails 5 cases and fires the S6 signal; disabling the format restorer fails 4 of 12 random orders; adding `copy_as_action.cpp` to the model library fails configure. (A reported "wrong row data in the clipped table fails the clipper cases" is not supported by the oracle's design and is not relied on.)

