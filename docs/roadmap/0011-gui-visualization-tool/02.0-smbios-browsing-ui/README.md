# Story 02.0 - SMBIOS Browsing UI: Entry Tree and Field Table

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete (2026-11-11)
**Depends on:** Story 01.0 (`smbios_gui` target, window/frame loop, whole-archive link)

## Scope

This story is the actual browsing tool: a view-model layer that turns one
live `smbios::SMBios` into a renderer-neutral tree (contract **GC1**,
`plan.md`), Dear ImGui rendering of that tree plus a per-entry field table,
and a "copy as..." action that reuses the existing `IOutputFormatter`
registry (milestone 0009) rather than embedding a second renderer - exactly
ADR 0003's Decision.

## Tasks

| #  | Document                                                              | Status         | Blocks |
|----|----------------------------------------------------------------------------|----------------|--------|
| 01 | [Entry tree and field view-model (GC1)](01-entry-tree-and-field-view-model.md) | ✅ Complete | 02     |
| 02 | [Tree and field-table rendering](02-tree-and-field-table-rendering.md)      | ✅ Complete | 03     |
| 03 | ["Copy as..." clipboard action](03-copy-as-clipboard-action.md)            | ✅ Complete | 04     |
| 04 | [Tests](04-tests.md)                                                       | ✅ Complete | -      |

## Key constraints

- **Consume `get_fields()` directly - never `render_to_description()`'s
  prose, and never a second decode path.** Per `plan.md`'s Key constraints
  and ADR 0003 DD1: Task 01's view-model is built from `SMBios` iteration,
  `SMBiosEntryFactory::create()` and `AbstractSMBiosEntry::get_fields()`
  only. It must never regex or otherwise re-parse `render_to_description()`'s
  output - that is precisely the `dmidecode | awk` anti-pattern milestone
  0009 exists to eliminate for every other consumer, and the ADR calls out
  by name.
- **The view model is Dear-ImGui-free.** Task 01's `view_model.hpp`/`.cpp`
  must not `#include` any Dear ImGui header, so it is unit-testable with
  plain Boost.Test and `SyntheticTableBuilder` fixtures, with no window or
  GPU context needed - see Task 04.
- **No structure is silently dropped.** A `DMIHeader` whose type has no
  registered `AbstractSMBiosEntry` (`SMBiosEntryFactory::create()` returns
  `nullptr`) still becomes a tree node (an empty field list, a label built
  from the raw type number) - per GC1's own doc comment. The milestone
  exit parity check (same structure count as `smbios --format text`) would
  silently pass for the wrong reason if unregistered types were simply
  skipped instead.
- **"Copy as..." reuses the registry, never a second writer.** Task 03
  calls `smbios::get_output_formatter(name)`/`render_document()`
  (`libsmbios/include/libsmbios/output_formatter.hpp`) for whichever name
  the user picked from `registered_output_formatter_names()` - it must not
  hand-roll JSON/XML/YAML/CSV text itself, mirroring milestone 0009's own
  R4/R5 reasoning (a new format is a registration, not a fork; a new
  *consumer* of an existing format must not become a second implementation
  of it either).
- **`FieldValue`'s four alternatives are rendered explicitly, via
  `std::visit`**, matching ADR 0003's Decision and the `examples/read_fields`
  precedent (`examples/read_fields/main.cpp`'s `overloaded` helper): a
  `std::string` as-is (already-decoded text, not re-escaped for display),
  a `uint64_t` with a decimal/hex toggle, a `bool` as a checkbox glyph, and
  a `std::vector<uint8_t>` as a hex dump. No alternative is silently
  dropped or stringified through a single generic path that loses the
  type information `get_fields()` deliberately carries.
- **No logging or persistence of field values**, matching milestone 0009
  Story 02.0's own rule (`output_formatter.hpp`'s doc comment) - this GUI
  displays and lets the user explicitly copy field values; it must not
  write any of them to a log file, crash report, or telemetry path of any
  kind. Firmware-controlled strings (serial numbers, UUIDs, asset tags) are
  potentially identifying.
- **Privacy in any checked-in screenshot or recording.** If a future PR for
  this story includes a screenshot for review, it must be taken against a
  synthetic fixture or a redacted live table - never a real machine's
  serial number/UUID/asset tag, matching the privacy discipline
  `examples/read_fields/main.cpp` and milestone 0009's doc-sample fixtures
  already apply.

## Files or directories created or modified by this story

- `smbios_gui/include/smbios/gui/view_model.hpp` (new; contract GC1).
- `smbios_gui/src/view_model.cpp` (new).
- `smbios_gui/include/smbios/gui/entry_tree_view.hpp`,
  `smbios_gui/src/entry_tree_view.cpp` (new; Task 02's rendering code).
- `smbios_gui/include/smbios/gui/copy_as_action.hpp`,
  `smbios_gui/src/copy_as_action.cpp` (new; Task 03).
- `smbios_gui/src/main.cpp` (modified; wires the view model and rendering
  code into Story 01.0's frame loop in place of the placeholder window).
- `tests/unit/` gains new test files (picked up by its existing
  `file(GLOB)`), Task 04.
