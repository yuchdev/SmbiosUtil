# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Final verification closing out Story 03.0 and milestone 0008.

## Files

No new test source. This task is the milestone's exit gate.

## Success criteria

- [ ] The agent produces at least one real, correctly-labeled report,
      committed under `docs/reviews/`, even if it says "nothing to
      report". The report records the reviewed HEAD SHA per upstream.
- [ ] `scripts/upstream_references.json` was not modified by the agent.
      Any baseline advance happened in a separate, human-reviewed PR.
- [ ] `python scripts/check_cversions_isolation.py` passes: no
      `.gitmodules`, and nothing tracked under `c-versions/`.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems
      across the full roadmap.
- [ ] Milestone 0008's `status.md` is updated to ✅ for Stories 01.0,
      02.0, and 03.0. Story 02.0 Task 02 stays marked superseded.
- [ ] `docs/roadmap/README.md`'s milestone table and Program overview
      describe milestone 0008 as the fetch-script design.
