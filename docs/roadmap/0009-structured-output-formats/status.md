# Milestone 0009 - Structured Output Formats - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|----------------------------------------------------|----------------|-------|
| 01.0  | Structured field model                             | 🔶 In progress (2/6 tasks) | 443 unit |
| 02.0  | Renderer registry and built-in formats             | ⬜ Not started | -     |
| 03.0  | YAML renderer and custom-format extensibility      | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request: the CLI (then `smbios_util`, now the `smbios` target) has exactly one
  output path (free-form text to stdout), confirmed by reading
  `main.cpp` directly - no existing milestone (0001-0008) assures
  structured output, so this is genuinely new scope, not a gap in an
  existing one.
- **2026-11-10 - JSON library decision: RESOLVED, hand-rolled (user-ratified).**
  `JsonFormatter` uses a hand-rolled emitter. No new dependency is added:
  no Boost.JSON, no nlohmann/json, no Boost.PropertyTree, and `vcpkg.json`
  is untouched. For the record, the pinned Boost is 1.92, so Boost.JSON's
  ≥ 1.75 minimum was met. It was still declined, for dependency hygiene
  and so that all three structured writers stay symmetrical. Story 02.0
  Task 02 no longer carries a "decide the library" step. This replaces
  the earlier "open for Story 02.0" bullet.
- **2026-11-10 - YAML library decision: RESOLVED, hand-rolled flat
  emitter (user-ratified).** There is no `yaml-cpp`, not even in a test
  binary. Every string scalar is double-quoted, including field-name keys,
  to avoid YAML implicit-typing traps (`NO`, `1.10`, `0x0A`, `~`). Only
  the fixed structural keys `type`/`fields` and the `uint64_t`/`bool`
  values are plain. This replaces the earlier "open for Story 03.0"
  bullet, and also the old spec's "quote only when ambiguous" rule.
  Shared escaping helpers (`formatter_escaping.hpp`: XML/JSON/YAML
  escapers plus `hex_encode`) are used by all writers, per the user's
  encouragement.
- **2026-11-10 - `get_fields()` is pure virtual on all entry types
  (user-ratified; C1 as written).** This covers all 45 types. During
  Story 01.0 only, a TEMPORARY non-pure default is allowed (see the
  Task 02 split below). It is gone at story exit.
- **2026-11-10 - Spec maintenance: milestone re-based onto the current
  codebase (no code written; status rows unchanged).** Gaps found and how
  each was handled:
  1. **Stale paths.**
     - The specs said `smbios/include/smbios/...`, `smbios/src/...` and
       `smbios_util/...`. They are re-pathed to the core library
       `libsmbios`: public headers under `libsmbios/include/libsmbios/`
       (included as `<libsmbios/...>`), sources in `libsmbios/src/`.
       Formatter headers go in a new `include/libsmbios/formatters/`, and
       their sources stay flat in `src/`, because the
       `libsmbios/src/CMakeLists.txt` glob is non-recursive. This mirrors
       `tables/`.
     - CLI references now point to the `smbios` target:
       `smbios/src/main.cpp`, `smbios/src/command_line_parser.cpp` and
       `smbios/include/smbios/command_line_parser.hpp`. The CSV example
       moves to `smbios/examples/`.
     - `FieldValue`/`NamedField` are consolidated into one header,
       `field_value.hpp`.
     - The stale `main.cpp:84-92` reference is corrected to
       `main.cpp:115-124`.
  2. **Scope is 45 types, not 11.** "Milestone 0004's eleven" is replaced
     by the 45 concrete `AbstractSMBiosEntry` subclasses that milestone
     0010 registered. That is exactly one `REGISTER_SMBIOS_ENTRY` each in
     `libsmbios/src/`, including `InactiveEntry` and
     `StringPropertyEntry`. There is no intermediate base class. Two test
     doubles also subclass it, plus a third that Story 01.0 Task 01 adds:
     `FakeTestEntry` (`tests/unit/smbios_entry_factory_test.cpp`) and
     `TestEntry` (`tests/unit/dmi_string_parsing_test.cpp`). The pure
     flip must update them. `smbios_c` does not subclass
     `AbstractSMBiosEntry` (consumer only), so it is unaffected.
  3. **Story 01.0 Task 02 is split into green batches.**
     - Task 01 adds `get_fields()` as a TEMPORARY non-pure virtual that
       returns `{}`.
     - Tasks 02-05 migrate 12/10/12/11 types by family: platform and
       firmware; processor and memory; slots, devices and power; probes
       and management.
     - Task 05 flips `get_fields()` to `= 0` in the same commit as its
       batch, and adds a 45-type coverage test.
     - The exit gate is renumbered to 06. The old
       `02-migrate-entries-to-structured-fields.md` and `03-tests.md` are
       superseded stubs awaiting `git rm`.
  4. **Library decisions** are resolved as recorded above.
  5. **Linking.**
     - `REGISTER_OUTPUT_FORMATTER` initializers in `libsmbios` run
       automatically in `smbios`, `smbios_c` and `tests/c_abi`, all of
       which force-load the whole archive.
     - `smbios_unit_test` does not force-load, so formatters define their
       members out of line and the unit tests reference the formatter
       classes. This is the same mechanism the entry tests already use.
     - The CSV example is compiled straight into `smbios`, so it needs no
       whole-archive flag.
  6. **Contract additions made during the re-base (Architect, not
     user-ratified).**
     - C2 gains `registered_output_formatter_names()`. R6's error listing
       and `--help` need it, and C2 previously had no enumeration API.
     - C1 gains the invariant that field names are unique within one
       entry, using 1-based index suffixes, so JSON/YAML mappings have no
       duplicate keys.
     - Escapers replace invalid UTF-8 with U+FFFD, because DMI strings
       are not guaranteed to be UTF-8.
     - `--format bogus` must exit non-zero via `stderr`, not via
       `usage()`, which exits 0 today.
  7. **R3-R6 are kept intact**, only re-pathed. R5's "zero changes" is
     made precise: no existing `libsmbios`/`smbios` *source* file is
     edited for the CSV example. The only edit is one
     `add_subdirectory(examples)` build-wiring line in
     `smbios/CMakeLists.txt`.
- **RATIFIED 2026-11-10 (user) - multi-entry document framing: option
  (a).** `IOutputFormatter` gains non-pure `begin_document()`,
  `entry_separator()` and `end_document()` hooks that return an empty string
  by default. This is additive to C2: `render(entry)` is unchanged. The CLI
  prints the `DMI version`/`Table size`/`Header ID` preamble only for
  `text`, so `text` stays byte-identical (R3). Option (b), JSON Lines, and
  option (c), `render_document(range)`, were rejected. Story 02.0 Task 01
  implements this; it no longer has to resolve it first.
- **RATIFIED 2026-11-10 (user) - the item 6 contract additions are
  accepted**: `registered_output_formatter_names()`, unique field names
  with 1-based suffixes, U+FFFD replacement for invalid UTF-8, and a
  non-zero exit for an unknown `--format`.
- **RATIFIED 2026-11-10 (user) - `--format` CLI smoke tests** are plain
  CTest `add_test` entries in `tests/integration/CMakeLists.txt`,
  classified as Integration. Add a note to `docs/dev/cpp_coding_standard.md`'s
  Testing section that CTest-level CLI smoke tests belong to that category.
