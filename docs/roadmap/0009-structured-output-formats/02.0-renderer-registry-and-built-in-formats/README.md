# Story 02.0 - Renderer Registry and Built-in Formats

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete (2026-11-10)
**Depends on:** Story 01.0 (`get_fields()` pure virtual on all 45 entry types)

> **Security-sensitive** (*amended 2026-11-10, security-auditor INFO-6*).
> This story serializes untrusted, firmware-controlled strings and bytes
> into machine-readable documents. A `--read-file` dump is fully
> attacker-controlled. The escaping, UTF-8, byte-cap and field-name rules
> in [Task 02](02-text-xml-and-json-renderers.md) are acceptance criteria,
> not style. The security tests in [Task 03](03-tests.md) gate story exit.
> Threat model: [docs/security/2026-11-10-structured-field-model.md](../../../security/2026-11-10-structured-field-model.md)
> (INFO-6, LOW-3, LOW-5). Request a security-auditor review in this story's `/pr-review`.

## Scope

This story implements:

- `IOutputFormatter` and its self-registering registry (contract **C2**,
  mirroring milestone 0004 Story 01.0's entry-factory pattern);
- the three built-in formats `text`, `xml` and `json`;
- the shared escaping helpers that the XML, JSON and (Story 03.0) YAML
  writers reuse.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Output formatter registry](01-output-formatter-registry.md)           | ✅ Complete (`f3447b2`) | 02     |
| 02 | [Text, XML and JSON renderers](02-text-xml-and-json-renderers.md)       | ✅ Complete (`edc344b`) | 03     |
| 03 | [Tests](03-tests.md)                                                   | ✅ Complete (`92a768e`, review fixes `c5bd074`) | -      |

## Key constraints

- **Mirrors the entry factory.** The registry's shape deliberately mirrors
  `libsmbios/include/libsmbios/smbios_entry_factory.hpp` and
  `libsmbios/src/smbios_entry_factory.cpp`:
  - the same self-registration macro idiom;
  - the same `std::logic_error` on a duplicate key;
  - the same "look up by string key, return `nullptr` on miss" contract.

  A reader who understands one already understands the other.
- **`text` is the only bypass.** `text` is the **only** formatter permitted
  to bypass `get_fields()` and call `render_to_description()` directly (R4's
  stated exception). This keeps exact backward compatibility with every
  existing `smbios` CLI invocation.
- **JSON is hand-rolled** (ratified 2026-11-10, see `status.md`). There is
  no Boost.JSON, nlohmann/json or Boost.PropertyTree, and `vcpkg.json` is
  not touched. The pinned Boost (1.92) would have allowed Boost.JSON; it
  was declined to keep the formatter layer dependency-free and
  symmetrical with the hand-rolled XML and YAML writers.
- **Escaping is shared.** XML, JSON and YAML escaping, invalid-UTF-8
  handling and the raw-byte hex encoding live in one internal helper
  (Task 02). Each writer does not carry its own copy.
- *Amended 2026-11-10 (security-auditor INFO-6/LOW-3):* **render exactly
  what `get_fields()` returns.**
  - Writers never clamp, drop or synthesise fields.
  - Placeholder records produced by unvalidated firmware counts (LOW-3) are
    fixed in the entry getters, as a follow-up, not in the renderers.
  - Field names are library constants, never firmware bytes.

  See Task 02's Constraints.
- **Registration and linking.** Built-in formatters live in `libsmbios`.
  They register at static-init time only when their object file is linked:
  - `smbios`, `smbios_c` and `tests/c_abi` already force-load `libsmbios`
    whole-archive, so registration happens automatically there.
  - `smbios_unit_test` does not force-load (`tests/unit/CMakeLists.txt`).
    Each formatter therefore defines at least one member out of line in its
    `.cpp`, and the unit tests reference the formatter class directly. That
    pulls its object, and therefore its `REGISTER_OUTPUT_FORMATTER`, into
    the test binary. This is the same mechanism the entry tests rely on.
- *Amended 2026-11-10 (downstream hook for
  [Story 03.0 Task 03](../03.0-yaml-renderer-and-custom-format-extensibility/03-examples-and-documentation.md)):*
  **Deterministic output.** Story 03.0 generates the user-guide samples by
  rendering a synthetic fixture through these formatters, and compares the
  result byte-for-byte with checked-in golden files on all three OSes.
  - No formatter may emit timestamps, host or locale data, or platform
    line endings. Always use `\n`.
  - Field order is `get_fields()` order.
  - Story 03.0 Task 03 also audits Doxygen completeness on this story's
    headers, so write the comments now.
- **No logging of field values.** Formatters and the registry never log
  or persist field values. Serial numbers, UUIDs and asset tags are
  potentially identifying, so they appear only in the rendered output the
  caller explicitly asked for. Error messages name the format or field,
  never its value.
- **Multi-entry document framing - RATIFIED 2026-11-10 as option (a)**
  (see `status.md` Notes & decisions). `IOutputFormatter` gains non-pure
  `begin_document()`/`entry_separator()`/`end_document()` hooks (each
  defaults to `""`). JSON wraps entries in an array, XML in a root
  element, and CSV writes its header from `begin_document()`. The CLI
  prints its preamble only for `text`.

## Files or directories created or modified by this story

- `libsmbios/include/libsmbios/output_formatter.hpp` and
  `libsmbios/src/output_formatter.cpp` (new; C2).
- `libsmbios/include/libsmbios/formatters/` (new directory):
  - `text_formatter.hpp`, `xml_formatter.hpp`, `json_formatter.hpp`;
  - `formatter_escaping.hpp`.

  The matching sources are flat in `libsmbios/src/`: `text_formatter.cpp`,
  `xml_formatter.cpp`, `json_formatter.cpp` and `formatter_escaping.cpp`.
  This mirrors the existing `tables/` headers / flat `src/*_entry.cpp`
  split, because `libsmbios/src/CMakeLists.txt`'s `file(GLOB SOURCES *.cpp ...)`
  is non-recursive.
- `libsmbios/src/CMakeLists.txt` gains one glob line for
  `include/libsmbios/formatters/*.hpp` (IDE/header listing only, matching
  the existing `tables/*.hpp` line). Re-run `cmake` after adding files.
- `tests/unit/` gains new test files (picked up by its `file(GLOB)`).
- `vcpkg.json` is **not** modified.
