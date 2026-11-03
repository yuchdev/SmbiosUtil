# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run Tasks 01-02's tests together and close out Story 02.0, before Story
03.0 adds YAML and the custom-format example on top of this now-proven
registry.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] All three built-in formatters pass their respective tests.
- [ ] `get_output_formatter("text")`/`"xml"`/`"json"` all resolve
      correctly via the registry (not just via direct instantiation in a
      test).
- [ ] Milestone 0009's `status.md` Story 02.0 row is updated to ✅.
