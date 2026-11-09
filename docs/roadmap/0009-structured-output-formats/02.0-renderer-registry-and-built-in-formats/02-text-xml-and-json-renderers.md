# 02 - Text, XML and JSON renderers

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Implement the three R3 built-in formatters, and make - and record - the
JSON library decision this task has been deferring.

## Files

- **Create** `smbios/include/smbios/text_formatter.hpp` / `.cpp`:
  ```cpp
  class TextFormatter : public IOutputFormatter {
  public:
      std::string render(const AbstractSMBiosEntry& entry) const override {
          return entry.render_to_description(); // R4's stated exception - exact backward compatibility
      }
      std::string format_name() const override { return "text"; }
  };
  REGISTER_OUTPUT_FORMATTER(TextFormatter);
  ```
- **Create** `smbios/include/smbios/xml_formatter.hpp` / `.cpp`: walks
  `entry.get_fields()`, emitting one `<field name="...">value</field>`
  per `NamedField` inside a `<structure type="...">` element - using
  `std::variant::visit` to render each `FieldValue` alternative (string
  as-is with XML-escaping, `uint64_t` as a decimal string, `bool` as
  `"true"`/`"false"`, `std::vector<uint8_t>` as a hex string) with no
  third-party XML library (hand-rolled, since the output shape is simple
  and flat enough that pulling in a full XML library for escaping +
  five element tags is not justified - escaping itself is a five-line
  function: `&`, `<`, `>`, `"` substitution).
- **Create** `smbios/include/smbios/json_formatter.hpp` / `.cpp`: same
  walk, emitting a JSON object - library choice **decided at the start of
  this task** (Boost.JSON, if the pinned vcpkg Boost baseline is ≥ 1.75;
  otherwise record why a different choice was made in `status.md`).

## Functions and Classes

- `TextFormatter`, `XmlFormatter`, `JsonFormatter` - new.

## Constraints

- `TextFormatter::render()` must produce byte-identical output to the
  current, pre-milestone `main.cpp`'s direct
  `render_to_description()` call (R3) - verified by diff, not assumed.
- `XmlFormatter`/`JsonFormatter` both consume `get_fields()` only (R4) -
  if either needs information `get_fields()` doesn't currently expose,
  that is a Story 01.0 gap to fix (adding a field), never a reason for
  this task to reach around `get_fields()` into the raw entry getters.

## Tests

- `tests/unit/text_formatter_test.cpp`: byte-identical diff against
  `render_to_description()` on real hardware.
- `tests/unit/xml_formatter_test.cpp`, `json_formatter_test.cpp`: against
  a synthetic entry (milestone 0006's `SyntheticTableBuilder`), confirm
  well-formed output (parse it back with a real XML/JSON parser and
  check round-trip field values) for every `FieldValue` alternative,
  including the XML-escaping edge cases (`&`, `<`, `>`, `"` in a vendor
  string).

## Success criteria

- [ ] `text` format is byte-identical to pre-milestone output.
- [ ] `xml`/`json` output parses successfully with a standard parser and
      round-trips every field correctly, including escaped characters.
- [ ] The JSON library decision is made and recorded in `status.md`.
