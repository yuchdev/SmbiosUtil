# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm that the fetch script and the isolation guard both work
end-to-end before Story 02.0's baseline-review procedure and Story 03.0's
agent are built on top of them.

## Files

No new test source beyond Task 01's and Task 02's manual checks and
fixtures. This task is the story's exit gate.

## Success criteria

- [ ] A fresh clone of this repository, followed by running
      `python scripts/fetch_upstream_references.py`, produces both working
      checkouts. Their printed HEAD SHAs match `git ls-remote`.
- [ ] `git status` is clean afterwards and no `.gitmodules` exists.
- [ ] The milestone 0001 CI matrix (all three OS jobs) runs the isolation
      guard and stays green.
- [ ] Milestone 0008's `status.md` Story 01.0 row is updated to ✅.
