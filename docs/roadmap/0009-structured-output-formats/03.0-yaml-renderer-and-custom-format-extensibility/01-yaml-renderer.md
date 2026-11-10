# 01 - YAML renderer

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `YamlFormatter` as a hand-rolled, flat YAML emitter. The library
question is settled: the 2026-11-10 ratified decision says "no
`yaml-cpp`", so it is not reopened here.

## Files

- **Create** `libsmbios/include/libsmbios/formatters/yaml_formatter.hpp`
  and `libsmbios/src/yaml_formatter.cpp`:
  ```cpp
  class YamlFormatter : public IOutputFormatter {
  public:
      std::string render(const AbstractSMBiosEntry& entry) const override;
      // emits one sequence item per entry:
      // - type: "BIOS Information"
      //   fields:
      //     "Vendor": "Dell Inc."
      //     "ROM Size": 65536
      //     "ISA is supported": true
      std::string format_name() const override;   // "yaml"
  };
  // in yaml_formatter.cpp only:
  REGISTER_OUTPUT_FORMATTER(YamlFormatter)
  ```
  Both members are defined out of line. See Story 02.0's linking note.
- **Quoting rule** (ratified):
  - Every string scalar is double-quoted, including every field-name key
    and the `type` value. Escaping uses Story 02.0's shared
    `yaml_double_quoted_escape()`. This avoids YAML's implicit-typing
    traps: a vendor string of `"NO"`, `"1.10"`, `"0x0A"` or `"~"`, or one
    containing `": "` or a leading `-`, never parses back as a bool,
    number or null.
  - Only the two fixed structural keys `type` and `fields` are plain.
  - A `uint64_t` is an unquoted decimal, and a `bool` is an unquoted
    `true`/`false`.
  - A `std::vector<uint8_t>` is a double-quoted `hex_encode()` string.
  - Concatenating per-entry renders yields one valid YAML sequence, so YAML
    needs no extra document framing. It is still subject to whatever Story
    02.0 Task 01 decided for the other formats.

## Functions and Classes

- `YamlFormatter` - new.

## Constraints

- No new vcpkg dependency and no `yaml-cpp`, in production code or in
  tests.
- The emitter is flat by design. It has no anchors, no multi-line block
  scalars and no nesting beyond `- type / fields / key: value`. Strings
  containing newlines are escaped (`\n`) inside double quotes, never
  emitted as block scalars.
- `YamlFormatter` reads `get_fields()` and `get_type()` only (R4). It
  includes no `libsmbios/tables/*.hpp` header.

## Tests

- `tests/unit/yaml_formatter_test.cpp` (new), run against synthetic
  entries built with `tests/unit/synthetic_table_builder.hpp`:
  - It asserts the exact emitted text for every `FieldValue` alternative.
  - It covers implicit-typing traps in string values and keys: `"NO"`,
    `"yes"`, `"1.10"`, `"0x0A"`, `"~"`, `"null"`, a value containing
    `": "`, a leading `-`, an embedded `"`/`\`, a newline, and invalid
    UTF-8. All must be double-quoted and correctly escaped.
- Dev-only round-trip: pipe the same renders through Python's
  `yaml.safe_load` (PyYAML, a developer-machine tool, not a build or CI
  dependency). Confirm every value round-trips with its intended type.
  Record the command used in this task's PR description.

## Success criteria

- [ ] YAML output parses with a real YAML parser (dev-only check) and
      round-trips every field with its intended type.
- [ ] No string scalar is emitted unquoted, as verified by the trap cases.
- [ ] `get_output_formatter("yaml")` resolves.
