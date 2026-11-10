# 02 - Tree and field-table rendering

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Render Task 01's `std::vector<EntryNode>` as Dear ImGui widgets: a tree/
list in a left pane (one entry per `EntryNode`, labeled with `label` and
`handle`), and, for whichever node is currently selected, a field table in
a right pane (one row per `FieldRow`, `FieldValue` rendered via
`std::visit`). This is the task that actually makes `smbios_gui` a
browsing tool rather than a placeholder window.

## Files

- **Create** `smbios_gui/include/smbios/gui/entry_tree_view.hpp`:
  ```cpp
  #pragma once
  #include <vector>

  #include <smbios/gui/view_model.hpp>

  namespace smbios::gui {

  /// Renders `nodes` as a selectable list (left pane) plus the selected
  /// node's field table (right pane), using ImGui calls only - no
  /// SMBios/SMBiosEntryFactory access of its own; `nodes` is the only
  /// input. Must be called once per frame, inside an existing ImGui frame
  /// (between ImGui::NewFrame() and ImGui::Render()).
  /// @param nodes the current table's view model, from build_entry_tree().
  /// @param[in,out] selected_index index into `nodes` currently shown in
  /// the field-table pane; clamped internally if `nodes` has shrunk (e.g.
  /// after a table refresh) since the last call.
  void render_entry_browser(const std::vector<EntryNode>& nodes, int& selected_index);

  } // namespace smbios::gui
  ```
- **Create** `smbios_gui/src/entry_tree_view.cpp`: a left `ImGui::BeginChild()`
  pane with one `ImGui::Selectable()` per node (label: `"{label} (handle 0x{handle, hex})"`),
  updating `selected_index` on click; a right pane showing the selected
  node's fields in an `ImGui::BeginTable()` with two columns (`Field`,
  `Value`), one row per `FieldRow`, cell text from a `std::visit` over
  `FieldValue`:
  - `std::string` -> the string as-is;
  - `uint64_t` -> its decimal form, with a per-row or per-pane toggle
    (e.g. a small `ImGui::Checkbox("Hex")`) switching to `0x`-prefixed hex;
  - `bool` -> a disabled `ImGui::Checkbox()` glyph (read-only: this is a
    display, not an editable form - see the story README's "read-only"
    constraint and `plan.md`'s milestone-level one);
  - `std::vector<uint8_t>` -> a hex dump (space-separated byte pairs, or
    `reuse smbios::hex_encode()` from
    `libsmbios/include/libsmbios/formatters/formatter_escaping.hpp` for the
    digit encoding, with the rendering code adding its own separators/
    wrapping - do not re-implement hex-digit encoding a second time).
- **Modify** `smbios_gui/src/main.cpp`: construct one `smbios::SMBios` at
  startup, call `smbios::gui::build_entry_tree()` once, and call
  `smbios::gui::render_entry_browser()` once per frame in place of Story
  01.0's placeholder `ImGui::Text()` call. If `bios.get_structures_count() == 0`
  (no source succeeded), render a visible "no SMBIOS table could be read"
  message instead of an empty tree with no explanation - matching the
  degrade-not-crash behavior `examples/read_fields/main.cpp` already
  establishes for this exact condition.

## Functions and Classes

- `smbios::gui::render_entry_browser()` - new.

## Constraints

- **No re-decoding.** This task's only access to `libsmbios` is through
  Task 01's `EntryNode`/`FieldRow` - it never calls
  `SMBiosEntryFactory::create()`, `get_fields()`, or any typed
  `AbstractSMBiosEntry` getter directly. If a field needs different
  formatting than `FieldValue`'s four alternatives already provide, that
  is a Task 01/GC1 change, not a workaround in this task.
- **Read-only widgets.** Every field-table cell is display-only; no
  `ImGui::InputText()`/editable widget appears anywhere in this task -
  see the story README's and `plan.md`'s "read-only" constraint.
- **No persistent widget-tree bookkeeping.** `render_entry_browser()` is
  called fresh every frame with the current `nodes`/`selected_index` state
  - it does not cache ImGui IDs or widget state across frames beyond what
  Dear ImGui's own per-ID state (e.g. scroll position) already handles
  automatically (ADR 0003 DD5).
- **One label per structure type, from Task 01's `entry_type_label()`.**
  Do not hand-roll a second type-to-label mapping inside the rendering
  code.

## Tests

None directly automated for the rendering calls themselves - see the story
README's Key constraints and [Task 04](04-tests.md)'s testing-scope note.
A manual run against the live machine, compared structure-for-structure
against `smbios --format text`, is this task's own functional check (and
the milestone's own exit-gate parity check, per `plan.md`).

## Success criteria

- [ ] Running `smbios_gui` on a real machine shows one tree entry per
      structure, in the same count as `smbios --format text`'s own
      "Header type" count for the same live table.
- [ ] Selecting a node shows its fields, matching that entry's own
      `get_fields()` result field-for-field (spot-checked against the
      `smbios --format json` output for the same structure).
- [ ] Every `FieldValue` alternative renders without a crash: confirm at
      least one structure of each of the four alternatives is exercised
      (e.g. a `std::string` field, a `uint64_t` field, a `bool` field from
      a bitmask property, and a `std::vector<uint8_t>` field such as a raw
      UUID) on the developer's own test machine or a synthetic fixture.
