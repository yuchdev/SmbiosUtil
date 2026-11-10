# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run Tasks 01-02's tests together and close out Story 02.0, before Story
03.0 adds YAML and the custom-format example on top of this now-proven
registry.

## Files

This task adds no new C++ test source beyond Tasks 01-02's additions. It
is the story's exit gate. The real-parser round-trip check for `xml` and
`json` is a dev-only step. Pipe a synthetic-entry render (or, on a
privileged machine, real output) through Python's `xml.etree.ElementTree`
and `json`. Do not add a parser dependency to any C++ target.

*Amended 2026-11-10 (security-auditor INFO-6):* this task now adds the
security cases below. They are new test sources, which the sentence above
predates. All of them are `tests/unit/` (deterministic, unprivileged,
synthetic) and need no new dependency.

## Security cases (amended 2026-11-10, security-auditor INFO-6)

Source: [threat model INFO-6, LOW-3 and LOW-5](../../../security/2026-11-10-structured-field-model.md).
The rules under test are in [Task 02](02-text-xml-and-json-renderers.md).

- **Test-local helpers** (new, `tests/unit/`, no new dependency):
  - `structured_output_validators.hpp`/`.cpp` holds two small structural
    validators. No XML or JSON parser is available to the unit binary
    (`vcpkg.json` carries only iostreams, program-options, test and
    date-time), so a round-trip parse is not an option here.
    - `bool is_well_formed_json(std::string_view)` is a recursive-descent
      check of the RFC 8259 grammar. It rejects raw U+0000-U+001F inside
      strings, invalid escapes, invalid UTF-8, trailing garbage and
      duplicate keys within one object.
    - `bool is_well_formed_xml(std::string_view)` checks for exactly one
      root element, balanced and correctly nested tags, quoted attribute
      values, and references limited to the five predefined entities. It
      also rejects any XML 1.0-forbidden code point (raw or as a
      `&#...;` reference), invalid UTF-8, and any `<!DOCTYPE`/`<!ENTITY`.
    - Each validator gets its own short positive and negative self-test, so
      a validator that accepts everything cannot pass silently.
  - `FieldsTestEntry` is a test double subclassing `AbstractSMBiosEntry`,
    like `TestEntry` in `dmi_string_parsing_test.cpp`. Its `get_fields()`
    returns a caller-supplied `std::vector<NamedField>`, which lets a test
    inject exact values that no real table can produce. String literals
    are always wrapped in `std::string(...)` (LOW-5, so a pre-P1957 library
    cannot bind them to `bool`). One case pins this:
    `NamedField{"x", std::string("y")}` must hold the `std::string`
    alternative.
- **Cases**, each run against `xml`, `json` and `text` (in
  `xml_formatter_test.cpp`, `json_formatter_test.cpp`,
  `text_formatter_test.cpp` and `formatter_escaping_test.cpp`):
  1. *Invalid UTF-8.* Use a synthetic BIOS Information entry whose vendor
     string is each of the following: lone `0x80`; truncated `0xC3`;
     overlong `C0 AF`; CESU surrogate `ED A0 80`; `F4 90 80 80` (above
     U+10FFFF); and Latin-1 `"Caf\xE9"`.
     - `xml`/`json`: each maximal invalid subpart becomes exactly one
       U+FFFD, the expected bytes are asserted exactly, and the document
       validates.
     - `text`: byte-identical to `render_to_description()`.
  2. *Control characters.* Use a value containing every byte 0x01-0x1F,
     plus 0x7F.
     - `json`: short forms where defined, `\u00XX` otherwise (ESC becomes
       `\u001b`).
     - `xml`: TAB/LF/CR survive, every other C0 byte becomes U+FFFD, and
       no `&#x..;` reference to a forbidden code point appears.
     - Both validate.
  3. *ANSI escape.* Use a value of `"\x1b[31mRED\x1b[0m"`. No raw `0x1B`
     appears in `xml` or `json` output.
     - `text`: passes through byte-identically. This is the known gap
       recorded in Task 02, and the test documents it rather than hiding it.
  4. *Bidi overrides and line separators.* Use a value containing U+202E,
     U+2066 and U+2069 and U+2028/U+2029.
     - Bidi characters pass through as raw UTF-8 in `xml` and `json`.
     - `json` escapes U+2028/U+2029 as ` `/` `.
     - Both validate.
  5. *XML-forbidden non-controls.* Use a value containing U+FFFE and
     U+FFFF (valid UTF-8). In `xml` both become U+FFFD; in `json` both pass
     through.
  6. *`uint64_t` max.* Use a `FieldsTestEntry` with `UINT64_MAX`, `0` and
     `2^53 + 1`.
     - `json` emits exactly `18446744073709551615`, `0` and
       `9007199254740993` as bare numbers (no quotes, no exponent, no
       rounding).
     - `xml` emits the same digits.
  7. *Empty `get_fields()`.* Use a synthetic `InactiveEntry` (type 126).
     - `json`: `"fields": {}`.
     - `xml`: an empty `<structure>` element.
     - Both validate standalone and inside a multi-entry document.
  8. *255-byte field.* Use a `FieldsTestEntry` with a 255-byte vector
     covering 0x00-0xFE.
     - Output is exactly the 510-character lowercase hex string in both
       formats, untruncated.
     - Also check a 0-byte vector, which gives an empty string.
  9. *Whole-document well-formedness with framing.* Render through the
     registry the same way the CLI does: `begin_document()`, then each
     `render(entry)` with `entry_separator()` between, then `end_document()`.
     - Run this over three documents: (a) zero entries, (b) one entry, and
       (c) a multi-entry document mixing a representative entry from each
       Story 01.0 batch, an `InactiveEntry`, and the hostile strings from
       cases 1-5.
     - The whole output must pass `is_well_formed_json` /
       `is_well_formed_xml`.
  10. *Field names are firmware-independent* (`FieldNamesAreFirmwareIndependent`).
      For every registered type, reuse the 45-type coverage structures from
      `get_fields_coverage_test.cpp`. Build each one twice, with two
      different string sets: all strings `"A"`, and all strings set to the
      hostile values from case 1.
      - Assert that the ordered `NamedField::name` sequences are identical
        between the two builds.
      - Assert that no name contains a byte outside printable ASCII.
  11. *Placeholder records are rendered, not clamped (LOW-3).* Use a
      synthetic Memory Channel entry whose device count exceeds what its
      Length holds.
      - Assert that `json`/`xml` emit exactly as many records as
        `get_fields()` returns, so the renderer neither drops nor adds any.
      - This pins the Task 02 stance. When the getter-side clamp lands as a
        follow-up, this test's expected count changes along with the
        getter.

## Success criteria

- [ ] All three built-in formatters pass their respective tests in
      `smbios_unit_test`.
- [ ] `get_output_formatter("text")`, `get_output_formatter("xml")` and
      `get_output_formatter("json")` all resolve correctly via the
      registry, not just via direct instantiation in a test.
- [ ] The `xml` and `json` outputs parse cleanly with Python's standard
      parsers.
- [ ] *Amended 2026-11-10 (security-auditor INFO-6):* security cases 1-11
      pass in `smbios_unit_test`, and the validators' negative self-tests
      reject what they should. The dev-only Python check also parses the
      case 9 multi-entry document.
- [ ] Milestone 0009's `status.md` Story 02.0 row is updated to ✅.
