# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm a clean checkout and the isolation guard both work end-to-end
before Story 02.0's bump workflow and Story 03.0's agent are built on top
of this foundation.

## Files

No new test source beyond Task 02's fixture - this is the story's exit
gate.

## Success criteria

- [ ] A fresh `git clone --recurse-submodules` succeeds and matches the
      pinned commits.
- [ ] The milestone 0001 CI matrix (all three OS jobs) runs the isolation
      guard and stays green.
- [ ] Milestone 0008's `status.md` Story 01.0 row is updated to ✅.
