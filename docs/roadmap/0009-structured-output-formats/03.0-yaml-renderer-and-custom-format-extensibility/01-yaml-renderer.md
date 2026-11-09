# 01 - YAML renderer

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `YamlFormatter`, resolving the hand-rolled-vs-`yaml-cpp`
decision `status.md` flagged as open, in favor of a minimal hand-rolled
emitter given the data shape (confirm this still holds once implementation
starts; revise the decision and record why if it doesn't).

## Files

- **Create** `smbios/include/smbios/yaml_formatter.hpp` / `.cpp`:
  ```cpp
  class YamlFormatter : public IOutputFormatter {
  public:
      std::string render(const AbstractSMBiosEntry& entry) const override;
      // emits:
      // - type: "BIOS Information"
      //   fields:
      //     Vendor: "Dell Inc."
      //     ROM Size: 65536
      //     ISA is supported: true
      std::string format_name() const override { return "yaml"; }
  };
  REGISTER_OUTPUT_FORMATTER(YamlFormatter);
  ```
  Scalar quoting follows YAML's own minimal rule: quote a string value
  only when it would otherwise be ambiguous (contains `: `, starts with a
  YAML special character, is empty, or could parse as a number/bool) -
  not every string, to keep output readable.

## Functions and Classes

- `YamlFormatter` - new.

## Constraints

- No new vcpkg dependency (per the leaning in `status.md`) unless
  implementation reveals a real need for YAML's fuller feature set
  (multi-line strings, anchors/references) that this project's flat
  `NamedField` shape doesn't actually require - if so, switch to
  `yaml-cpp` and update `status.md`'s decision record rather than
  fighting a hand-rolled emitter past its design envelope.

## Tests

- `tests/unit/yaml_formatter_test.cpp` (new): against a synthetic entry,
  parse the emitted YAML back with a real YAML parser (a small, separate
  dev-only tool/script - e.g. Python's `yaml.safe_load` via a test-only
  script, or a vcpkg-available C++ YAML parser used *only* in the test
  binary, never in `smbios` itself) and confirm round-trip field values,
  including the quoting-ambiguity edge cases (a vendor string that looks
  like a number, one containing `": "`).

## Success criteria

- [ ] YAML output parses successfully with a real YAML parser and
      round-trips every field correctly.
- [ ] The hand-rolled-vs-library decision is confirmed or revised, with
      the outcome recorded in `status.md`.
