# 01 - Entry tree and field view-model (GC1)

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **GC1** (`plan.md`): `FieldRow`, `EntryNode`, and
`build_entry_tree()` - the one, Dear-ImGui-free function that turns a live
`smbios::SMBios` into the data Task 02's rendering code walks. This is the
layer Task 04's unit tests exercise directly.

## Files

- **Create** `smbios_gui/include/smbios/gui/view_model.hpp`:
  ```cpp
  #pragma once
  #include <cstdint>
  #include <string>
  #include <vector>

  #include <libsmbios/field_value.hpp>

  namespace smbios {
  class SMBios;
  } // namespace smbios

  namespace smbios::gui {

  /// One row of an entry's field table - a direct copy of one NamedField.
  struct FieldRow {
      std::string name;
      smbios::FieldValue value;
  };

  /// One tree node: one DMIHeader, labeled, with its decoded fields (empty
  /// if SMBiosEntryFactory::create() returned nullptr for this type).
  struct EntryNode {
      size_t type;
      uint16_t handle;
      std::string label;
      std::vector<FieldRow> fields;
  };

  /// Builds one EntryNode per structure in `bios`, in SMBios iteration
  /// order. Pure function: no Dear ImGui call, no I/O beyond what
  /// SMBios/SMBiosEntryFactory/get_fields() already do.
  /// @param bios the table to walk; must outlive the returned nodes'
  /// construction (copies out of it, so the result itself does not borrow).
  /// @return one node per structure, same count and order as iterating
  /// `bios` directly.
  /// @throws std::bad_alloc only.
  std::vector<EntryNode> build_entry_tree(smbios::SMBios& bios);

  /// Looks up the human-readable label for a SMBios::SMBiosHandler type
  /// value (e.g. 0 -> "BIOS Information"), or "Unknown (0xNN)" for a type
  /// with no name in that enum (OEM-specific types 0x80-0xFF, or any
  /// future spec type this project has not named yet).
  /// @param type the raw DMIHeader::get_type() value.
  /// @return the label text, never empty.
  std::string entry_type_label(size_t type);

  } // namespace smbios::gui
  ```
- **Create** `smbios_gui/src/view_model.cpp`: implements `build_entry_tree()`
  by iterating `bios` (`for (const smbios::DMIHeader& header : bios)`),
  calling `smbios::SMBiosEntryFactory{}.create(header, bios.get_smbios_version(), bios.get_table_base() + bios.get_table_size())`,
  and - when non-null - copying `entry->get_fields()` into `FieldRow`s; when
  null, emitting a node with an empty `fields` vector. `entry_type_label()`
  is a `switch` over `smbios::SMBios::SMBiosHandler` with one `case` per
  named value, defaulting to the `"Unknown (0xNN)"` form.

## Functions and Classes

- `smbios::gui::FieldRow`, `smbios::gui::EntryNode` - new structs.
- `smbios::gui::build_entry_tree()`, `smbios::gui::entry_type_label()` -
  new free functions.

## Constraints

- **No Dear ImGui include, anywhere in this task's two files.** This is
  what makes Task 04's unit tests possible without a window or GPU
  context; a single stray `#include <imgui.h>` would defeat the whole
  point of separating this layer out.
- **`build_entry_tree()`'s node count always equals
  `std::distance(bios.begin(), bios.end())`** - never fewer (an
  unregistered type is still a node, per the story README's Key
  constraints) and never more.
- **Field order matches `get_fields()`'s own order exactly** - `FieldRow`s
  are appended in the order `get_fields()` returns them, with no
  resorting, deduplication, or filtering. (Milestone 0009's C1 already
  guarantees unique names within one entry's result; this task adds no
  further processing on top of that guarantee.)
- **`entry_type_label()` only needs the 46 names `SMBios::SMBiosHandler`
  already declares** (`BIOSInformation` through `StringProperty`, plus
  `Inactive`/`EndOfTable`) - do not hand-maintain a second, duplicate
  name table; if a future SMBIOS type is added to that enum, this
  function's `switch` gains one more `case` in the same change, not a
  separate pass.

## Tests

See [Task 04](04-tests.md) - this task's own correctness is proven there,
via `SyntheticTableBuilder` fixtures covering a registered type, an
unregistered type, and a multi-entry table, checking both node count and
per-node field content against direct `get_fields()` calls.

## Success criteria

- [ ] `view_model.hpp`/`.cpp` compile standalone against `libsmbios`'s
      public headers alone, with zero Dear ImGui dependency.
- [ ] `build_entry_tree()`'s result, for a real live table, has the same
      length as `std::distance(bios.begin(), bios.end())`.
- [ ] Every `FieldRow` in a node whose type has a registered entry class
      matches that class's own `get_fields()` call, field-for-field, in
      the same order.
