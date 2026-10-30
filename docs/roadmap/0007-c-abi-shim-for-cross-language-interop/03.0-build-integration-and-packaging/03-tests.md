# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the build/packaging integration works end-to-end on all three
platforms, closing out Story 03.0 and milestone 0007.

## Files

No new test source - this is the story's (and milestone's) exit gate.

## Success criteria

- [ ] `smbios_c` builds, installs, and is discoverable via pkg-config on
      Windows, Linux and macOS.
- [ ] The milestone 0001 CI matrix includes `smbios_c` in its build (and,
      where unprivileged, its test run).
- [ ] CLAUDE.md and the root README gain a short mention of the C ABI
      (what it's for, where its header lives) alongside the existing
      C++ library/CLI description.
- [ ] `python scripts/check_doc_links.py docs/` reports zero problems
      across the full, now-seven-milestone roadmap.
- [ ] Milestone 0007's `status.md` is updated to ✅ for all three stories.
- [ ] `docs/roadmap/README.md`'s milestone table and Program overview gain
      milestone 0007.
