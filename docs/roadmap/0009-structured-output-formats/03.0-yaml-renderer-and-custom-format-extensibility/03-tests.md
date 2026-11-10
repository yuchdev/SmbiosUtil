# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Final verification, closing out Story 03.0 and milestone 0009. All five
format paths (`text`/`xml`/`json`/`yaml`/`csv`) must work end-to-end on
real hardware, through the actual `smbios` CLI, not just through unit
tests against synthetic fixtures.

## Files

This task adds no new test source; it is the milestone's exit gate. It
makes the documentation updates listed in the success criteria.

## Success criteria

- [ ] `smbios --format <name>` produces valid, correctly structured output
      on real hardware for every one of `text`, `xml`, `json`, `yaml` and
      `csv`. This must hold across Windows, Linux and macOS (the native
      sources from milestone 0005).
- [ ] `text` output is confirmed byte-identical to the tool's behaviour
      before this milestone.
- [ ] The `smbios_cli_*` CTest entries from Task 02 pass. The two
      unknown-format tests must pass in CI on every OS.
- [ ] `CLAUDE.md`, the root `README.md` and `docs/cli-usage.md` gain a
      short mention of `--format`, the registry-based extension point
      (`REGISTER_OUTPUT_FORMATTER`) and the CSV example. The
      `CLAUDE.md` mention also covers its whole-archive or
      direct-object linking requirement.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems
      across the full roadmap.
- [ ] Milestone 0009's `status.md` is updated to ✅ for all three stories.
- [ ] `docs/roadmap/README.md`'s milestone table and Program overview
      include milestone 0009.
