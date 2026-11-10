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

## Success criteria

- [ ] All three built-in formatters pass their respective tests in
      `smbios_unit_test`.
- [ ] `get_output_formatter("text")`, `get_output_formatter("xml")` and
      `get_output_formatter("json")` all resolve correctly via the
      registry, not just via direct instantiation in a test.
- [ ] The `xml` and `json` outputs parse cleanly with Python's standard
      parsers.
- [ ] Milestone 0009's `status.md` Story 02.0 row is updated to ✅.
