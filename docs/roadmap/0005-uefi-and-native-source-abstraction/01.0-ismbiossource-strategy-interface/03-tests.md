# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the interface and chain reproduce today's exact behavior on every
current platform before Stories 02.0-04.0 add real new sources on top of
it, closing out Story 01.0.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] Full existing functional suite passes unmodified on Windows, Linux
      and macOS (whichever is the only source available on each until
      later stories land).
- [ ] `render_to_description()`'s output is byte-identical before/after
      this story on each platform.
- [ ] Milestone 0005's `status.md` Story 01.0 row is updated to ✅.
