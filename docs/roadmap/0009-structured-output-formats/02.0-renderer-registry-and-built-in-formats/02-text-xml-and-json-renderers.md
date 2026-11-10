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
- *Amended 2026-11-10 (security-auditor INFO-6):* escaping rules, made
  precise. Source: [threat model INFO-6](../../../security/2026-11-10-structured-field-model.md#info-info-6-requirements-story-020030-serializers-must-meet-string-and-byte-values-are-firmware-controlled).
  Firmware strings may hold any byte 0x01-0xFF and may be as long as the
  remaining table.
  - **Strict UTF-8 first, in every format.** Each escaper validates strictly
    (RFC 3629) *before* any format-specific escaping. Overlong encodings,
    UTF-16 surrogates U+D800-U+DFFF (CESU-8), code points above U+10FFFF
    and truncated sequences all count as invalid. Each maximal invalid
    subpart becomes exactly one U+FFFD (`EF BF BD`), following the Unicode
    "maximal subpart" practice, so the output is deterministic. Expose this
    as one shared internal step (for example
    `std::string sanitize_utf8(const std::string&)` in
    `formatter_escaping.cpp`). Do not give each escaper its own decoder (CWE-176).
  - **XML 1.0: replace, never escape.** After UTF-8 sanitising,
    U+0000-U+0008, U+000B, U+000C, U+000E-U+001F, U+FFFE and U+FFFF are
    *replaced* with U+FFFD. XML 1.0 forbids these code points even as
    character references, so `&#x1B;` is not an option. TAB, LF and CR pass
    through. Then escape `&` `<` `>` `"` `'` as the five predefined
    entities. DEL and C1 (U+007F-U+009F) are legal XML 1.0 and pass
    through. The writer never emits a DOCTYPE, an entity declaration or a
    processing instruction other than the optional `<?xml version="1.0" encoding="UTF-8"?>`
    declaration from `begin_document()`.
  - **JSON.** After UTF-8 sanitising, escape `"` and `\`, and escape
    every code point in U+0000-U+001F. Use the short forms `\b \f \n \r \t`
    where they exist, and `\u00XX` (lowercase hex) for the rest, so ESC
    becomes `\u001b`. Also escape U+2028 and U+2029 as ` `/` `.
    That is valid JSON and keeps the output safe to embed in JavaScript.
    All other code points, including DEL, C1 and the bidi overrides, are
    emitted as raw UTF-8.
  - **YAML** (used by Story 03.0). The rule is in
    [Story 03.0 Task 01](../03.0-yaml-renderer-and-custom-format-extensibility/01-yaml-renderer.md).
    `yaml_double_quoted_escape()` escapes every non-printable YAML 1.2
    code point, not only C0.
  - **Bidi overrides and ANSI sequences** (U+202A-U+202E, U+2066-U+2069,
    ESC `[`...). After the rules above, ESC is escaped (JSON, YAML) or
    replaced (XML). The bidi code points are valid characters in all three
    formats and are passed through as data. Neutralising them for display
    is the consumer's job. The structured writers guarantee only that they
    cannot break document structure (CVE-2021-42574 is a display-spoofing
    issue, not a parsing one).
  - **`hex_encode` cap: none needed, by construction.** Every
    `std::vector<uint8_t>` field is sliced from one structure's formatted
    area, which `DMIHeader::length` (a `uint8_t`) bounds to at most 255
    bytes. Output is therefore at most 510 hex characters, and
    `hex_encode` neither truncates nor caps. The Doxygen comment on
    `hex_encode` states this bound. A future getter that sources bytes from
    *outside* one formatted area (for example a reassembled multi-structure
    payload) must reopen this decision and add a cap before it lands.
    Bytes are never emitted as a raw string in any format.
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
    - *Amended 2026-11-10 (security-auditor INFO-6):* this stays the rule.
      The full value is always emitted textually as exact decimal digits
      (for example `18446744073709551615`), never through a `double` and
      never in exponent form, so no information is lost on the wire.
      RFC 8259 §6 allows the number, and arbitrary-precision consumers
      (Python `json`, Go `json.Number`, jq 1.7+) read it exactly. The
      `json_formatter.hpp` Doxygen comment and `docs/cli-usage.md`'s
      `--format json` note (written in Story 03.0) state that JavaScript
      `JSON.parse` and other IEEE-double consumers lose precision above
      2^53 (64-bit memory-error addresses, extended sizes). Whether to
      string-encode these values instead is an open question, not decided
      here.
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
- *Amended 2026-11-10 (security-auditor INFO-6):* **field names come from
  code, never from firmware.** Every `NamedField::name` is a
  library-controlled constant: a literal, a constant-map label, or one of
  those with an integer index suffix (C1). No firmware byte may reach a
  name. Firmware data reaches only *values*.
  - Writers place names only in escaped *value* positions: the XML
    `name="..."` attribute value through `xml_escape()`, and the JSON object
    key through `json_escape()`. A name never becomes an XML element or
    attribute *name* (CWE-91). That escaping is defence in depth, not
    permission to relax the rule.
  - A future `get_fields()` that wants to derive a name from a DMI string
    (for example a vendor-defined label) is a C1 contract change and needs
    its own ADR.
  - Enforced by the `FieldNamesAreFirmwareIndependent` test in
    [Task 03](03-tests.md).
- *Amended 2026-11-10 (security-auditor LOW-3):* **renderers render exactly
  what `get_fields()` returns.** A firmware repeated-record count that is
  not checked against the structure length can make `get_fields()` emit
  placeholder records (for example `"Handle 7": 0`). Writers do not clamp,
  drop, deduplicate or reinterpret those records. They hold no
  per-entry-type knowledge (R4), so they cannot tell a placeholder from a
  real zero.
  - The affected getters are Memory Channel, Firmware Inventory, Event Log
    descriptors, MCHI protocol records and Firmware Language.
  - The fix belongs in those entry getters, as a follow-up outside this
    story: clamp to `min(count, records that fit in Length)` and expose the
    declared count as its own field. It must change `render_to_description()`
    and `get_fields()` together, so text and fields stay in agreement. It is
    tracked in `status.md`'s Carried forward list.
- *Amended 2026-11-10 (security-auditor INFO-6):* **`text` is not
  sanitised in this story.** R3 requires `text` to be byte-identical to
  `render_to_description()`, so ESC/C1 sequences in firmware strings still
  reach the terminal unchanged (CWE-150). This is a pre-existing gap on
  `master`. Fixing it changes `render_to_description()` for every entry
  type, so it is out of scope here and listed as an open question in
  `status.md`'s Carried forward list.

## Tests

- `tests/unit/formatter_escaping_test.cpp`. Table-driven cases for each
  escaper:
  - `&`, `<`, `>`, `"`, `'`, `\`;
  - control characters;
  - an empty string;
  - valid multi-byte UTF-8, which must pass through;
  - invalid UTF-8, which must become U+FFFD.

  It also covers `hex_encode` for empty input and multi-byte input.
  *Amended 2026-11-10 (security-auditor INFO-6):* the full security case
  list is in [Task 03](03-tests.md#security-cases-amended-2026-11-10-security-auditor-info-6).
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
- [ ] *Amended 2026-11-10 (security-auditor INFO-6):* every escaping rule
      above (strict UTF-8, XML replacement set, JSON `\u00XX` and
      U+2028/U+2029, exact `uint64_t` digits) is covered by a passing
      Task 03 security case.
