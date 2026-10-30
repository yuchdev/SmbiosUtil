# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm C++20 plus `-Werror` plus the new sanitizer job together don't
change observable behavior, closing out Story 01.0 before Story 02.0's
bugfixes rely on this gate to catch regressions.

## Files

No new test source - this is the story's exit gate.

## Success criteria

- [ ] Full existing functional test suite passes unmodified on GCC, Clang
      and MSVC under C++20/`-Werror`.
- [ ] The sanitizer CI job is green on a clean run.
- [ ] `docs/roadmap/0002-cpp20-modernization-and-bugfixes/status.md`'s
      Story 01.0 row is updated to ✅.
