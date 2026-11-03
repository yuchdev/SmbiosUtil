# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Final verification closing out Story 03.0 and milestone 0009: all five
format paths (`text`/`xml`/`json`/`yaml`/`csv`) work end-to-end on real
hardware, through the actual CLI, not just through unit tests against
synthetic fixtures.

## Files

No new test source - this is the milestone's exit gate.

## Success criteria

- [ ] `smbios_util --format <name>` for every one of `text`, `xml`,
      `json`, `yaml`, `csv` produces valid, correctly-structured output
      on real hardware across Windows/Linux/macOS (milestone 0005's
      platforms).
- [ ] `text` output is confirmed byte-identical to the tool's behavior
      before this milestone.
- [ ] CLAUDE.md and the root README gain a short mention of `--format`
      and the registry-based extension point.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems
      across the full, now-nine-milestone roadmap.
- [ ] Milestone 0009's `status.md` is updated to ✅ for all three stories.
- [ ] `docs/roadmap/README.md`'s milestone table and Program overview gain
      milestone 0009.
