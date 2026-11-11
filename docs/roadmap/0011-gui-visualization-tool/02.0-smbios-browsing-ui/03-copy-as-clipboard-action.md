# 03 - "Copy as..." clipboard action

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-11; PASS; manual clipboard-paste check vs `smbios --format json` still owed)

## Objective

Add a "copy as..." action to the selected entry's field-table pane: a
dropdown of every name in `smbios::registered_output_formatter_names()`
(`text`, `xml`, `json`, `yaml`, `csv` as of milestone 0009), and a button
that renders the currently-selected entry through
`smbios::get_output_formatter(name)` and copies the result to the OS
clipboard via `ImGui::SetClipboardText()`. This is ADR 0003's Decision's
own "copy as..." feature, and the reason this milestone needs no second
JSON/XML/YAML/CSV writer of its own.

## Files

- **Create** `smbios_gui/include/smbios/gui/copy_as_action.hpp`:
  ```cpp
  #pragma once
  #include <cstddef>

  namespace smbios {
  class AbstractSMBiosEntry;
  } // namespace smbios

  namespace smbios::gui {

  /// Renders a format-name dropdown (populated from
  /// registered_output_formatter_names()) plus a "Copy" button. On click,
  /// renders `entry` through the selected formatter and copies the result
  /// to the OS clipboard via ImGui::SetClipboardText(). No-op (renders
  /// nothing) if `entry` is null - e.g. the selected node's type has no
  /// registered AbstractSMBiosEntry.
  /// @param entry the currently-selected entry, or nullptr.
  void render_copy_as_action(const smbios::AbstractSMBiosEntry* entry);

  } // namespace smbios::gui
  ```
- **Create** `smbios_gui/src/copy_as_action.cpp`: an `ImGui::Combo()` (or
  `ImGui::BeginCombo()`/`ImGui::Selectable()` pair) over
  `smbios::registered_output_formatter_names()`, remembering the last
  selected name across frames in a function-local `static` (display state
  only, not table data - consistent with ADR 0003 DD5's "no retained
  widget-tree lifecycle" since this is the same kind of transient UI state
  Dear ImGui's own examples keep in a local `static`, not a second
  persistent object graph), and an `ImGui::Button("Copy")` that, on click,
  calls `smbios::get_output_formatter(name)->render(*entry)` and
  `ImGui::SetClipboardText(result.c_str())`.

## Functions and Classes

- `smbios::gui::render_copy_as_action()` - new.

## Constraints

- **No second renderer.** This task must not format JSON/XML/YAML/CSV text
  itself under any circumstance - every byte copied to the clipboard comes
  from `IOutputFormatter::render()` (or `render_document()` for a
  multi-entry "copy whole table as..." variant, if added - see Open
  questions below). This is the one hard rule ADR 0003's Decision and this
  story's README both state for this feature.
- **Single-entry scope for this task.** Copies the currently-selected
  entry only (`formatter->render(*entry)`), not document framing
  (`begin_document()`/`end_document()`) - a single entry copied to a
  clipboard for pasting into another tool does not need a whole document's
  framing. A "copy whole table as..." action using `render_document()` is
  an open question (below), not a requirement of this task.
- **No field value in any log or error message.** If `entry` is null or a
  formatter lookup somehow fails (it should not, since the dropdown is
  populated from the same registry), show a neutral status message (e.g.
  "nothing to copy") - never attempt to recover by printing partial field
  data to a debug console.
- **Clipboard content is exactly what `render()` returns** - no
  GUI-specific wrapping, trimming, or re-escaping added on top.

## Open questions

- Should a second action copy the *whole table* (`render_document()` over
  every `EntryNode`'s underlying entry) rather than only the selected one?
  Deferred to this task's own implementation discretion or a follow-up -
  not required for this story's exit gate, which only needs single-entry
  copy to prove the registry-reuse pattern end to end.

## Tests

- `tests/unit/` cannot drive `ImGui::SetClipboardText()` without a Dear
  ImGui context (GLFW's clipboard backend needs an active window) - see
  [Task 04](04-tests.md)'s testing-scope note. What *can* be unit-tested,
  and should be, is the non-ImGui part: that
  `smbios::get_output_formatter(name)->render(*entry)` for every name in
  `smbios::registered_output_formatter_names()` returns non-empty,
  well-formed output for a synthetic entry - this is really a
  `libsmbios`-level assertion already covered by milestone 0009's own test
  suite, so Task 04 only needs a thin confirmation that this task's code
  calls the registry correctly (e.g. via a small refactor exposing the
  "render selected entry in format X" logic as a free function Task 04 can
  call without a GLFW window - see Task 04 for the exact shape).

## Success criteria

- [ ] The dropdown lists exactly `registered_output_formatter_names()`'s
      current contents (`text`, `xml`, `json`, `yaml`, `csv`), sorted, with
      no hardcoded format list duplicating that function.
- [ ] Clicking "Copy" with `json` selected and pasting the clipboard
      elsewhere reproduces exactly what `smbios --format json`'s output
      contains for that one entry (allowing for `render_document()`'s
      multi-entry array framing, which single-entry `render()` does not
      add).
- [ ] No field value appears in any log, console, or error message this
      task's code produces.

## As-built notes (2026-11-11)

- **Amended by the 02.0 threat model** (reqs 11-14): entries are never created per frame - `create_entry_for_node()` runs only in the Copy click handler (re-checks index/type/handle, then `create()`); default format `json`, non-json formats show a static note; clipboard is byte-exact `IOutputFormatter::render()` output; >1 MiB and embedded-NUL output are refused; no field values/`e.what()` logged; ImGui-free `render_entry_as(const AbstractSMBiosEntry&, std::string_view) -> std::optional<std::string>` lives in `libsmbios_gui_model` (`copy_as.{hpp,cpp}`).
- **Spec corrections:** (1) the GUI dropdown lists 4 formats (`json`, `text`, `xml`, `yaml`), **not** `csv` - the csv formatter is compiled only into the `smbios` CLI target (`smbios/examples/csv_formatter.cpp`); the "exactly `registered_output_formatter_names()`" criterion holds. (2) Signature is `render_copy_as_action(SMBios&, const EntryNode&, size_t node_index) noexcept` (not `const AbstractSMBiosEntry*`), and `render_entry_browser` gained an `SMBios&` parameter. (3) `render_entry_as` does not catch exceptions; the GUI click handler does. Task 04's tests and any test target must whole-archive-link libsmbios so the formatters are registered.
- **Still owed:** manual paste check - Copy with `json` must equal `smbios --format json` for that entry.

