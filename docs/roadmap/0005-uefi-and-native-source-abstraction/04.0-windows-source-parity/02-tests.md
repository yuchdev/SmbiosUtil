# 02 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm Windows behavior is unchanged, closing out Story 04.0 and - since
every story in this milestone depends only on Story 01.0 and the four
stories can land in any order relative to each other - potentially closing
out milestone 0005 if Stories 02.0/03.0 have also already landed.

## Files

No new test source - this is the story's exit gate.

## Success criteria

- [ ] Full existing functional suite passes unmodified on Windows.
- [ ] `source_name()` reports the adapted native source's name on a
      successful run (not the legacy-scan fallback), confirming the chain
      ordering and the native source's success path both work.
- [ ] Milestone 0005's `status.md` Story 04.0 row is updated to ✅. If
      this is the last of the four stories to close, add the milestone
      closeout note: all four platforms (Windows, Linux, macOS, plus the
      universal legacy-scan fallback) now run through one shared
      `ISmbiosSource` chain abstraction, and CLAUDE.md/README's
      platform-support statements are updated to match reality.
