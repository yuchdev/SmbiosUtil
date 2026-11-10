# 02 - Text, XML and JSON renderers

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Implement the three R3 built-in formatters and the shared escaping helpers
they (and Story 03.0's YAML writer) use. JSON is hand-rolled, which was
ratified on 2026-11-10 and is not revisited here.

## Files

- **Create** `libsmbios/include/libsmbios/formatters/formatter_escaping.hpp`
  and `libsmbios/src/formatter_escaping.cpp`. These are free functions in
  `namespace smbios`, each with a Doxygen comment:
  ```cpp
  std::string xml_escape(const std::string& s);           // & < > " ' ; invalid XML 1.0 chars and invalid UTF-8 -> U+FFFD
  std::string json_escape(const std::string& s);          // " \ and U+0000-U+001F as \uXXXX (\n \t etc. short forms); invalid UTF-8 -> �
  std::string yaml_double_quoted_escape(const std::string& s); // " \ and C0 controls as YAML escapes; invalid UTF-8 -> �
  std::string hex_encode(const std::vector<uint8_t>& bytes);   // lowercase, no separators
  ```
  DMI strings are not guaranteed to be valid UTF-8 (some vendors use
  Latin-1 or emit stray bytes). Every escaper therefore replaces invalid
  sequences with U+FFFD deterministically, so the output stays a
  well-formed document.
- **Create** `libsmbios/include/libsmbios/formatters/text_formatter.hpp`
  and `libsmbios/src/text_formatter.cpp`:
  ```cpp
  class TextFormatter : public IOutputFormatter {
  public:
      std::string render(const AbstractSMBiosEntry& entry) const override; // returns entry.render_to_description() - R4's stated exception
      std::string format_name() const override;                             // "text"
  };
  // in text_formatter.cpp only:
  REGISTER_OUTPUT_FORMATTER(TextFormatter)
  ```
  Both members are defined out of line in the `.cpp`. Referencing the
  class from a test then links the object file, and with it the
  registration (see the Story README's linking note).
- **Create** `libsmbios/include/libsmbios/formatters/xml_formatter.hpp`
  and `libsmbios/src/xml_formatter.cpp`:
  - The formatter walks `entry.get_fields()` and emits one
    `<field name="...">value</field>` per `NamedField` inside a
    `<structure type="...">` element (`type` = `entry.get_type()`).
  - Each `FieldValue` alternative is rendered with `std::visit`:
    - a string is passed through `xml_escape()`;
    - a `uint64_t` becomes a decimal string;
    - a `bool` becomes `true` or `false`;
    - a `std::vector<uint8_t>` becomes `hex_encode()`.
  - There is no third-party XML library.
- **Create** `libsmbios/include/libsmbios/formatters/json_formatter.hpp`
  and `libsmbios/src/json_formatter.cpp`. The formatter walks the same
  fields and emits one JSON object per entry:
  `{"type": "<get_type()>", "fields": {"<name>": <value>, ...}}`.
  - Strings go through `json_escape()`.
  - A `uint64_t` is a bare JSON number. Note that values above 2^53 lose
    precision in IEEE-double parsers. This is accepted and documented in
    the header comment.
  - A `bool` is a bare `true`/`false`.
  - Bytes become a `hex_encode()` string.
  - Field-name uniqueness (C1) guarantees no duplicate object keys.
- Wrap every document per the framing decision from Task 01 (recorded in
  `status.md`).

## Functions and Classes

- `TextFormatter`, `XmlFormatter`, `JsonFormatter` - new.
- `xml_escape`, `json_escape`, `yaml_double_quoted_escape`, `hex_encode` -
  new. These are shared, and Story 03.0 reuses `yaml_double_quoted_escape`.

## Constraints

- **`text` is byte-identical.** `TextFormatter::render()` must produce
  output byte-identical to the current `smbios/src/main.cpp`'s direct
  `entry->render_to_description()` call (R3). Verify this by diff, not by
  assumption. `main.cpp`'s surrounding text (the `DMI version`/`Table
  size` preamble, `Header ID = N` lines, and the trailing `'\n'` after each
  entry) stays in `main.cpp` for the `text` path. Story 03.0 must not move
  or change it.
- **Structured formats read `get_fields()` only.** `XmlFormatter` and
  `JsonFormatter` consume `get_fields()` and `get_type()` only (R4). If
  either needs information `get_fields()` doesn't expose, that is a Story
  01.0 gap to fix by adding a field. It is never a reason to reach around
  `get_fields()` into the concrete entry getters. Neither `.cpp` may
  include any `libsmbios/tables/*.hpp` header.
- **No new dependencies.** No new vcpkg dependency, and no change to
  `vcpkg.json`.

## Tests

- `tests/unit/formatter_escaping_test.cpp`. Table-driven cases for each
  escaper:
  - `&`, `<`, `>`, `"`, `'`, `\`;
  - control characters;
  - an empty string;
  - valid multi-byte UTF-8, which must pass through;
  - invalid UTF-8, which must become U+FFFD.

  It also covers `hex_encode` for empty input and multi-byte input.
- `tests/unit/text_formatter_test.cpp`. Build synthetic entries (via
  `tests/unit/synthetic_table_builder.hpp`) for a representative type from
  each Story 01.0 batch. Assert that `TextFormatter().render(e) ==
  e.render_to_description()` byte for byte, and that
  `get_output_formatter("text")` resolves.
- `tests/unit/xml_formatter_test.cpp` and `tests/unit/json_formatter_test.cpp`,
  against synthetic entries:
  - Confirm well-formed output for every `FieldValue` alternative,
    including escaping edge cases (`&`, `<`, `>`, `"` and an invalid UTF-8
    byte in a vendor string).
  - Do this with a small test-local parser or structural checks, so that
    the unit binary gains no new dependency. A full round-trip through a
    real XML/JSON parser runs in Story 02.0 Task 03 (via a dev-only
    Python `xml.etree`/`json` check), never inside `libsmbios`.

## Success criteria

- [ ] `text` format is byte-identical to the pre-milestone output.
- [ ] `xml` and `json` output is well-formed and round-trips every field
      correctly, including escaped characters and invalid UTF-8
      replacement.
- [ ] No formatter source includes a concrete entry header. `vcpkg.json`
      is unchanged.
