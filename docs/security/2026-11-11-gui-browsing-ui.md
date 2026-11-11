# Threat Model - smbios_gui browsing UI (milestone 0011, Story 02.0) - 2026-11-11

Pre-implementation review. Scope: `docs/roadmap/0011-gui-visualization-tool/02.0-smbios-browsing-ui/`
(README + Tasks 01-04), `plan.md` GC1, ADR 0003. The Story 01.0 follow-ups recorded in
`docs/roadmap/0011-gui-visualization-tool/status.md:21` are folded in here. **Every coder
briefing for Story 02.0 must include the "Per-task requirements" section below.** Where a
requirement here conflicts with a task spec, this document wins. The known conflicts are
marked [OVERRIDES SPEC].

## Assets & trust boundaries

- **Assets:** firmware identifiers such as serial numbers, UUIDs, asset tags and OEM strings
  (PII-class device identifiers); the integrity of what the user sees; the desktop session
  that runs the GUI process; the OS clipboard.
- **B1 - firmware/hypervisor -> `SMBios` table buffer.** The bytes are hostile: malicious
  firmware, a VM-supplied table, or a corrupt table.
- **B2 - `get_fields()` -> GC1 view model.** String values can be any byte 0x01-0xFF. They
  contain no NUL (the parser splits on NUL), are not guaranteed to be UTF-8, and one string
  can be as long as the remaining table. Byte vectors are at most 255 bytes today (see
  `2026-11-10-structured-field-model.md` INFO-6).
- **B3 - view model -> Dear ImGui widgets.** Format-string APIs, label/ID parsing (`##`,
  `###`), and per-frame layout cost.
- **B4 - `IOutputFormatter::render()` -> OS clipboard -> any paste target.** Paste targets
  include terminals, spreadsheets and chat. The `text`, `xml` and `csv` formatters are
  documented as not terminal-safe (`text_formatter.hpp:22-29`, SR-7). `json` is
  terminal-safe.
- **Privilege:** Linux sysfs is root-only. A desktop GUI must not be run as root to get
  around that.

## Findings

### [HIGH] G-1 Firmware string used as an ImGui format string or label (CWE-134, CWE-20)
- Vector: `ImGui::Text(value.c_str())`, `Selectable(value)`, `TreeNode(value)` and similar
  calls in `entry_tree_view.cpp` / `copy_as_action.cpp`. A `%n`/`%s` value reaches ImGui's
  vsnprintf, so `%s` reads arbitrary memory. ImGui's own formatter has no `%n`, but the
  platform vsnprintf it falls back to may support it. A value containing `##`/`###` hides
  part of the label and rewrites the widget ID.
- Impact: info leak or crash from a crafted table, and UI spoofing.
- Mitigation: firmware-derived text is passed only to `TextUnformatted(begin, end)`, or to
  `Text("%s", p)` where it cannot contain NUL. It is never passed as a format string or as a
  widget label/ID. Labels are built only from `entry_type_label()` (a constant) and the
  numeric handle.

### [MEDIUM] G-2 ID collisions from duplicate handles/types (CWE-694)
- Vector: a hostile table can repeat `(type, handle)`. Then `"{label} (handle 0x..)"` is
  not unique, ImGui merges the IDs, and selecting or hovering one row affects the others.
- Mitigation: call `PushID(int node_index)` / `PopID()` around every per-node widget, and
  `PushID(row_index)` around every field row. Never derive an ID from the handle.

### [MEDIUM] G-3 Unbounded string, field and node rendering (CWE-400, CWE-770)
- Vector: one string can be as large as the table. A 32-bit SMBIOS 3 table can also hold
  10^5-10^6 four-byte structures. ImGui lays out every visible item on every frame.
- Impact: frame-time collapse, UI freeze, memory growth.
- Mitigation: cap displayed values at 4096 bytes, cutting at a UTF-8 code-point boundary,
  and append ` ... [N bytes truncated]`. Cap hex dumps at 4096 bytes the same way. Use
  `ImGuiListClipper` for the node list and for any field table with more than a few hundred
  rows. Build the tree once at startup and never once per frame.

### [MEDIUM] G-4 Control characters, newlines, invalid UTF-8 and bidi in displayed values (CWE-150, CWE-176, CWE-451)
- Vector: an embedded `\n` can draw a fake extra row (for example a forged
  "Serial Number"). Invalid UTF-8 is interpreted however ImGui chooses. ImGui shows
  C0/C1/bidi controls as missing glyphs, which is harmless but inconsistent.
- Mitigation: add an ImGui-free helper, `display_text(const std::string&) -> std::string`
  (for example in `view_model.hpp`). It (1) truncates per G-3, (2) runs
  `smbios::sanitize_utf8()` (`formatter_escaping.hpp:57`, reuse it, do not reimplement),
  (3) replaces C0 (including TAB/CR/LF), DEL, C1 and U+2028/2029/202A-202E/2066-2069 with
  U+FFFD or a visible `\xNN`. **[OVERRIDES SPEC]** The README says strings render "as-is,
  not re-escaped". The view model and the clipboard keep the raw value. Only the on-screen
  projection is sanitized.

### [MEDIUM] G-5 Clipboard paste injection and size (CWE-150, CWE-74, CWE-770)
- Vector: `text`/`csv`/`xml` output keeps raw ESC/CSI/CR/LF. If it is pasted into a
  terminal, those bytes run as control sequences or commands ("pastejacking"). One entry
  can render to megabytes.
- Mitigation: make `json` the default in the dropdown, because it is terminal-safe. When
  the selected format is not terminal-safe, show a short static note next to the button
  ("output may contain raw control characters"). Do not rewrite the output: the spec
  requires the clipboard to hold exactly what `render()` returned. Refuse to copy more than
  1 MiB and show a neutral status ("entry too large to copy"). Copy only when the user
  clicks, never automatically. Clipboard managers keep a history of copied text: this is an
  accepted residual risk of an explicit user action.

### [MEDIUM] G-6 Exceptions escaping the frame loop / unbalanced ImGui stack (CWE-248, CWE-755)
- Vector: `SMBios()`, `SMBiosEntryFactory::create()` and `get_fields()` can throw (for
  example `bad_alloc`, or a `runtime_error` from an entry constructor). If that happens
  between `Begin` and `End`, ImGui is left mid-frame, and the RAII teardown runs on an
  inconsistent context.
- Mitigation: GC1 says "never throws except `bad_alloc`", so enforce that.
  `build_entry_tree()` wraps each entry's `create()` + `get_fields()` in
  `try { } catch (const std::exception&)` and still emits the node with empty `fields` (plus
  a `decode_failed` flag or label suffix, without `e.what()`). Wrap `SMBios` construction
  and `build_entry_tree()` in `main()` in a try/catch that prints a fixed message and
  exits 1. Run them before the frame loop, never inside a frame. Rendering code must not
  throw: no `.at()`, and no allocation-heavy work per frame.

### [MEDIUM] G-7 Entry lifetime for "Copy as..." (CWE-416)
- Vector: Task 03 takes `const AbstractSMBiosEntry*`, but `EntryNode` owns none. The
  obvious shortcuts are to keep a raw pointer to a temporary `unique_ptr`, or to call
  `create()` on every frame. The first dangles. The second re-decodes on every frame and
  breaks Task 02's "no re-decoding" rule. Every entry's `DMIHeader::data` points into
  `SMBios::table_buffer_`.
- Mitigation: `main()` owns one `SMBios`. Use `std::unique_ptr` and do not copy it.
  Either (a) keep a `std::vector<std::unique_ptr<AbstractSMBiosEntry>>` parallel to the
  nodes, built once and destroyed before the `SMBios`, or (b) call `create()` only inside
  the Copy click handler, scoped to that handler. Declare the `SMBios` before every entry
  container.

### [MEDIUM] G-8 Running the GUI as root (CWE-250)
- Vector: Linux sysfs is root-only, so users will be tempted to `sudo smbios_gui`. That
  runs GLFW, the GL driver and the X11/Wayland client code as root, all of which is a large
  attack surface.
- Mitigation: no doc, error message or hint may suggest sudo/root/admin for the GUI. On
  POSIX, if `geteuid() == 0`, show a persistent in-window warning banner. Do not refuse to
  run. When the view is empty, show "no SMBIOS table could be read (this source may need
  elevated privileges)" with no "run as root" advice. A privilege-separated reader or a
  `--read-file` dump is the right long-term path (follow-up, out of scope).

### [LOW] G-9 Empty or failed source path keyed on the wrong count
- Vector: Task 02 checks `get_structures_count() == 0`. That count can be non-zero while
  `begin() == end()` on malformed input (`smbios.hpp:157-170`).
- Mitigation: **[OVERRIDES SPEC]** decide on `nodes.empty()`, which is the same as
  `std::distance(begin(), end()) == 0`. An empty `nodes` must never be indexed. Clamp
  `selected_index`: if it is `< 0` or `>= size`, reset it to 0. When the size is 0, render
  the message and no table. Do the comparisons as `size_t` after checking for negative
  values.

### [LOW] G-10 Combo state dangling / null formatter (CWE-476, CWE-825)
- Mitigation: the function-local `static` keeps a `std::string` name or an index, never a
  `const char*` into the vector `registered_output_formatter_names()` returns by value.
  Check `get_output_formatter(name)` for `nullptr`, and show "nothing to copy" if it is
  null. Never print field values, `render()` output or `e.what()` to stderr. Task 03 also
  bans field values in any log.

### [LOW] G-11 Read-only guarantee and checkbox binding
- Mitigation: render `bool` as `BeginDisabled(); bool v = value; Checkbox("##v", &v);
  EndDisabled();` under `PushID`. Use no `InputText`. Keep `io.IniFilename = nullptr`
  (`main.cpp`), so nothing is persisted. Load no fonts from runtime paths and keep the
  embedded default font: stb_truetype has had heap overflows on crafted fonts, for example
  CVE-2022-25514.

## Per-task requirements

**Task 01 (view model)**
1. Make `build_entry_tree()` exception-isolated per entry (G-6). A throwing entry still
   produces exactly one node. Rethrow only `std::bad_alloc`.
2. Add the ImGui-free `display_text()` helper (G-3 cap + G-4 sanitizing) here, so Task 04
   can unit-test it. The view model itself keeps raw values.
3. Copy everything (no borrowing). Document that `SMBios` must outlive any entry objects
   created from it (G-7).

**Task 02 (rendering + `main.cpp`)**
4. Firmware text goes only through `TextUnformatted` / `"%s"`, after `display_text()`. It
   is never a format string, label or ID (G-1, G-4).
5. `PushID(index)` per node and per field row. Never use the handle as an ID (G-2).
6. Use `ImGuiListClipper` for the nodes and for long field tables. Build the tree once,
   outside the frame loop (G-3).
7. Base the empty view on `nodes.empty()`. Clamp `selected_index` safely (G-9).
8. Construct `SMBios`/the tree inside try/catch before the loop, with a fixed message and
   exit 1. Nothing in a frame throws (G-6).
9. Show the euid==0 warning banner. Never advise root (G-8).
10. Use read-only disabled checkboxes and no editable widgets (G-11).

**Task 03 (Copy as...)**
11. Ownership: a parallel `unique_ptr` entry vector built once, or `create()` scoped to the
    click handler. Never a per-frame `create()` (G-7).
12. Default the dropdown to `json`, and show the static note for the other formats. Keep
    the clipboard byte-exact. Refuse to copy more than 1 MiB (G-5).
13. The combo `static` holds a `std::string` or an index. Null-check the formatter. Put no
    field values, output or `e.what()` in any log or stderr (G-10).
14. Expose `render_entry_as(const AbstractSMBiosEntry&, const std::string& name) ->
    std::optional<std::string>` (ImGui-free, applies the size cap) so Task 04 can test it.

**Task 04 (tests, `tests/unit/`)**
15. Hostile-string test: a synthetic entry whose strings include `%n%s%x`, `##`, `###id`,
    `\n`, ESC `[2J`, U+009B, U+202E, invalid UTF-8 (`\xC0\xAF`, lone `\xED\xA0\x80`) and
    a 64 KiB string. Assert that `build_entry_tree()` returns the raw bytes unchanged and
    that `display_text()` output is valid UTF-8, contains no C0/C1/DEL/bidi, is at most the
    cap plus the suffix, and is idempotent.
16. Malformed-table test: oversized `length`, missing terminator, and
    `get_structures_count() != distance(begin, end)`. Assert that the node count equals
    `distance(begin, end)` and that nothing crashes (run under the `sanitizers` CI job).
17. Duplicate-handle fixture: two structures with the same type and handle produce two
    distinct nodes.
18. `render_entry_as()`: every registered name returns non-empty output for a synthetic
    entry, an unknown name returns `nullopt`, and an oversized entry returns `nullopt`.
19. Test the empty `SMBios` (no table) path: `build_entry_tree()` returns empty and does
    not throw.
20. No real-machine serials or UUIDs in fixtures, screenshots or the `status.md` parity
    record (counts only).

## Verdict: PASS_WITH_FOLLOWUP

The story is pre-implementation, so there are no CRITICAL findings and nothing to block.
G-1 (HIGH) becomes a merge-blocking item at code review: any firmware-derived value
reaching an ImGui format-string or label parameter is a BLOCK. The remaining items are
acceptance criteria for the tasks above. Fixes go to `cpp-expert` (Tasks 01-03) and tests
to `testing-expert` (Task 04, items 15-20). Out of scope: a privilege-separated reader and
`--read-file` (G-8).

## Post-implementation verification - 2026-11-11

Reviewed the uncommitted working tree: `smbios_gui/{include/smbios/gui,src}/*`,
`tests/unit/gui_*`, and the root and `tests/unit` CMake changes. Every ImGui call was
audited by hand.

| Req | Result | Evidence |
|-----|--------|----------|
| 1 | PASS | `view_model.cpp:119-126`: rethrows `bad_alloc`, otherwise `decode_failed`, no `what()`. |
| 2 | PASS (see P-1) | `view_model.cpp:189-226`: reuses `sanitize_utf8`. The C0/DEL/C1/2028-202E/2066-2069 set is correct. The cut at `keep` is in bounds because `size > 4096 > keep`. The suffix reserve keeps the result at or under the cap, so the function is idempotent. |
| 3 | PASS | `FieldRow`/`EntryNode` own their data. The lifetime is documented in `view_model.hpp:21-27`. |
| 4 | PASS | Every label, ID and format argument is a literal (`##node`, `##entry_list`, `##entry_fields`, `##field_table`, `Field`/`Value`, `##value`, `Hex`, `##copy_as`, `##format`, `##format_item`, `Copy`). Firmware text reaches ImGui only through `TextUnformatted(b,e)` (`entry_tree_view.cpp:63,101,116,165`). The `BeginCombo` preview is a registry key, not firmware data. |
| 5 | PASS | `PushID(int)` is used per node (`:52`), per field row (`:96`), per table (`:193`) and per combo item. No ID uses the handle. |
| 6 | PASS (see P-1) | Clipper on the node list (`:78`) and on field tables over 256 rows (`:139`). The tree is built once (`main.cpp:182-188`). |
| 7 | PASS | `entry_tree_view.cpp:206-214`: the sign is checked before the `size_t` comparison. |
| 8 | PASS | `main.cpp:182-188`: fixed message and exit 1. The browser and the copy action are `noexcept`, so `bad_alloc` terminates (accepted). ImGui Begin/End, BeginChild/EndChild, BeginTable/EndTable, BeginCombo/EndCombo and Push/Pop calls are balanced on every path. |
| 9 | PASS | `main.cpp:120-135`: banner wording gives no privilege advice. `kNoTableText` matches G-8. A grep found no sudo/root advice in the GUI sources or docs. |
| 10 | PASS | Disabled checkbox bound to a local copy (`:107-110`). No `InputText`. |
| 11 | PASS | `create_entry_for_node()` is called only from the Copy click handler (`copy_as_action.cpp:87`). It re-checks index, type and handle (`copy_as.cpp:46-57`). `bios` is declared before `nodes` in `main.cpp:180`, so it is destroyed last. |
| 12 | PASS | json is the default (`copy_as.cpp:28-36`). Static note for the other formats (`copy_as_action.cpp:161`). 1 MiB cap with refusal (`copy_as.cpp:22`). NUL is refused, not truncated (`:99`). The clipboard receives exactly what `render()` returned. |
| 13 | PASS | The combo state is an index into a static copy of the names. The formatter is null-checked. No values, output or `what()` go to stderr (the only stderr text is fixed strings plus GLFW's own error description). |
| 14 | PASS | `render_entry_as()` is ImGui-free and applies the cap. |
| 15-19 | PASS | `gui_view_model_test.cpp:356` (every hostile input listed in req 15, plus TAB/CR/DEL/U+2028/U+2066, with an idempotence check in `check_display_safe`), `:449-568` (malformed tables), `:569` (duplicate handles), `:596-626` (empty table), `gui_copy_as_test.cpp:91-225` (render_entry_as). ASan/UBSan clean. |
| 20 | PASS | Fixtures are synthetic. The `status.md` parity record holds counts and type codes only. |
| G-11 | PASS | `IniFilename = nullptr` and `NoSavedSettings` are set. There are no `AddFontFrom*` or `LogToFile` calls. |

New code surface: `libsmbios_gui_model` is always built. It has no GUI dependencies and no
install rule, so it adds no new external input. The whole-archive link in the unit test
target only fills the registries inside the test binary and has no effect on shipped
artifacts.

### [LOW] P-1 display_text() runs over the whole string every frame (CWE-400)
- Vector: `entry_tree_view.cpp:100,112` call `display_text()` on every visible row in every
  frame. `view_model.cpp:191-204` sanitizes the entire raw string, allocating up to 3x its
  size twice, before it applies the 4096-byte cap. G-4 step order said "truncate, then
  sanitize". Nothing limits table size (`linux_sysfs_source.cpp:138`
  `read_entire_file`), and the field-table path for 256 rows or fewer formats every row in
  every frame. The cap therefore bounds layout cost but not the cost of computing the
  projection.
- Impact: a multi-MB firmware string slows frames down for as long as that entry is
  selected. It is local-only and does not cause memory growth.
- Mitigation (follow-up, `cpp-expert`): compute the display strings once, at tree-build time,
  in a parallel cache. Alternatively, pre-cut the raw input to about
  `kDisplayTextMaxBytes + 4` bytes before `sanitize_utf8` and compute N from the raw
  length. Add a test that times or bounds a 16 MiB string (`testing-expert`).

### [LOW] P-2 "Terminal-safe" json can still carry bidi controls to the clipboard (CWE-451)
- Vector: `json_escape` (`libsmbios/src/formatter_escaping.cpp:196-222`) escapes C0, DEL
  and C1 but leaves U+202A-202E and U+2066-2069 as raw characters. The same Trojan Source
  class as CVE-2021-42574 (disputed) applies when the text is pasted into editors or chat.
  It is not a terminal-control vector. This behaviour predates this story.
- Mitigation (follow-up, optional): `\u`-escape the bidi controls in `json_escape`. This is
  lossless and still valid JSON. Alternatively, document the residual risk.

### [INFO] P-3 render_entry_as renders in full before checking the cap
- `copy_as.cpp:21`: on a click, memory briefly grows to about 6x the entry size because of
  JSON escaping. A `bad_alloc` from that path terminates the process through `noexcept`,
  which was accepted under G-6.

### Post-implementation verdict: PASS_WITH_FOLLOWUP
G-1 is clean, so the HIGH item does not block. There are no CRITICAL or HIGH findings. The
P-1 and P-2 follow-ups are not merge-blocking.
