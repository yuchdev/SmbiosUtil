# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-11)

## Objective

Close out Story 03.0 and, with it, this milestone: confirm `smbios_gui`
builds cleanly through CI on all three OSes, with no run-step added, and
that the documentation footprint this story owns (the one-line
`smbios_gui` pointer in `docs/cli-usage.md`/`CLAUDE.md`) is in place.

## Testing-scope call

This story's own "tests" are build/CI confirmation, not new Boost.Test
cases - restating the pattern from Story 01.0/02.0's own tests tasks:
there is nothing behavioral in CMake/CI wiring itself to unit-test: either
`smbios_gui` builds on a given platform's CI runner or it does not, and
that is already what the `build` job's exit status reports. No headless
run-and-screenshot step exists in this milestone (see the story README's
Key constraints and Non-goals) - running `smbios_gui` interactively, on at
least one real OS, to confirm it opens and shows real SMBIOS data remains
a manual step, the same one Story 02.0, Task 04 already performs for its
own milestone-exit parity check.

## Files

No new test source.

## Tests

- A real CI run (not a local dry run) on a push/PR touching this
  milestone's branch, showing the `build` job green on `ubuntu-latest`,
  `windows-latest` and `macos-latest`, with `smbios_gui` present in each
  run's `Build` step output.

## Success criteria

- [ ] CI is green on all three OSes with `smbios_gui` in the build graph.
- [ ] `docs/cli-usage.md` and root `CLAUDE.md` each mention `smbios_gui`
      (one line each, matching milestone 0007's `smbios_c` precedent) and
      `/link-check` (`scripts/check_doc_links.py docs/`) is clean.
- [ ] Milestone 0011's `status.md` Story 03.0 row, and the milestone's own
      exit summary, are updated to ✅, cross-referencing Story 02.0, Task
      04's parity-check result.

## As-built notes (2026-11-11)

- **CI-green criterion met:** CI run `43052749581` on branch `milestone-0011` completed `success` with all 8 jobs green: `build` and `sanitizers` (which run the 42 new GUI unit cases, ASan/UBSan clean on Linux/macOS) on Linux, macOS and Windows as before, plus the new build-only `gui-build` on `ubuntu-24.04` (`linux-gui`), `macos-latest` (`macos-gui`) and `windows-latest` (`windows-gui`), each configuring and building `smbios_gui` and reporting the binary. No CI step runs the GUI.
- **Docs criterion met:** `CLAUDE.md` ("Building the GUI" bullet) and `docs/cli-usage.md` each mention `smbios_gui`, with a pointer to the manual test plan (`docs/test/manual-testing.md`); `scripts/check_doc_links.py docs/` is clean (254 files).
- **Status/exit record:** this milestone's parity gate result (26 = 26 registered nodes on the maintainer's Mac) is recorded under Story 02.0, Task 04.
- **Cost of getting there:** the first four `gui-build (ubuntu)` runs failed for CI-environment reasons (Ubuntu mirror DNS, a `-e` bug in the failure-output script, missing autotools for vcpkg's X11 build, no step timeouts) - see Task 02's "CI debugging findings".
- **Still not automated / owed:** the real-window checks in `docs/test/manual-testing.md` (maintainer), and Story 04.0's headless and local-only test tiers.

