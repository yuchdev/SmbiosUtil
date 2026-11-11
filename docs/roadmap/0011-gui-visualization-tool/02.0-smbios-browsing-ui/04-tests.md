# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Close out Story 02.0 with real, hermetic unit coverage of the one layer
that is actually unit-testable (Task 01's view model), plus this
milestone's exit-gate parity check against `smbios --format text` - the
same bar ADR 0003's own Validation/Rollout section sets.

## Testing-scope call

Restated from Story 01.0, Task 03, and applied concretely to this story's
own deliverables:

- **Unit-testable (and tested here): the view model.** `build_entry_tree()`
  and `entry_type_label()` (Task 01) take no Dear ImGui dependency and no
  window/GPU context, so they are tested exactly like any other
  `libsmbios`-adjacent logic in this project - `BOOST_AUTO_TEST_CASE`s
  against `smbios::test::SyntheticTableBuilder` fixtures (milestone 0006),
  run in CI on every push, no root/admin/display needed.
- **Not unit-tested: the Dear ImGui rendering calls (Tasks 02-03).**
  `render_entry_browser()`/`render_copy_as_action()` issue real ImGui
  widget calls that need a live `ImGuiContext` (and, for the clipboard
  action, GLFW's own clipboard backend, which needs a window). Driving
  these headlessly to assert on pixel output or the internal widget-ID
  tree is disproportionate effort for a diagnostic tool with one
  contributor, and is explicitly out of scope for this milestone (see
  `plan.md`'s Non-goals) - they are verified by the manual runs each
  task's own Success criteria describe, and by this task's parity check
  below.
- **Verified manually, not by an automated test:** the "copy as..."
  clipboard round-trip (requires an OS clipboard and a human to paste and
  compare).

## Files

- **Create** `tests/unit/gui_view_model_test.cpp`
  (`BOOST_AUTO_TEST_SUITE(GuiViewModelUnitTests)`):
  - `BuildEntryTreeMatchesStructureCount` - a `SyntheticTableBuilder`
    fixture with several structures of a registered type (e.g. BIOS
    Information, Memory Device) plus one of an unregistered/OEM type;
    `build_entry_tree()`'s result length equals
    `std::distance(bios.begin(), bios.end())`.
  - `BuildEntryTreeFieldsMatchGetFields` - for each node whose type is
    registered, its `fields` equal (name-for-name, value-for-value, same
    order) a direct `entry->get_fields()` call on the same structure.
  - `BuildEntryTreeUnregisteredTypeYieldsEmptyFields` - the OEM/
    unregistered-type node has an empty `fields` vector and a label in the
    `"Unknown (0xNN)"` form, not a crash or a skipped node.
  - `EntryTypeLabelCoversEveryNamedHandlerValue` - `entry_type_label()`
    returns a non-"Unknown" label for every named `SMBios::SMBiosHandler`
    value (mirrors milestone 0009's "45-type coverage test" precedent,
    `tests/unit/get_fields_coverage_test.cpp`, applied to this task's own
    label table instead of the entry factory).
- `smbios_gui/CMakeLists.txt` needs no new test wiring - `view_model.cpp`
  is linked into `tests/unit/`'s existing `smbios_unit_test` target via its
  own `file(GLOB)` only if it is placed under `tests/unit/` directly, or
  (preferred, keeping `smbios_gui/`'s sources with the rest of that
  target) by having `tests/unit/CMakeLists.txt` add
  `smbios_gui/src/view_model.cpp`/`smbios_gui/include` to its own sources/
  include path, the same way an entry `.cpp` from `libsmbios/src/` is
  referenced by name rather than copied. Record whichever layout is chosen
  in `status.md`.

## Tests

See Files above - `gui_view_model_test.cpp` is this task's entire new
automated-test surface.

## Success criteria

- [ ] `smbios_unit_test` is green with the new suite included, on all
      three OSes.
- [ ] **Milestone exit parity check** (ADR 0003's own Validation/Rollout
      bar, restated in `plan.md`'s Rationale): on a real machine, the
      number of tree nodes `smbios_gui` shows equals the number of
      "Header type" occurrences `smbios --format text` prints for the same
      live table. Record the comparison (counts only, no field values) in
      `status.md`.
- [ ] The testing-scope call above is recorded in `status.md`'s Notes &
      decisions, so a future contributor does not re-raise "where are the
      UI tests?" as an open question.
- [ ] Milestone 0011's `status.md` Story 02.0 row is updated to ✅.
