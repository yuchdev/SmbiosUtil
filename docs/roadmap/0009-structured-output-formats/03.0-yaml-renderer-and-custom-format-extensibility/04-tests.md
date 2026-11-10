# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 03 ([Examples and documentation](03-examples-and-documentation.md))

> *Renumbered 2026-11-10* from `03-tests.md`. The user added examples and
> documentation scope as Task 03, so this exit gate moved to 04. See
> [status.md](../status.md). The documentation-update criteria that used to
> live here moved into Task 03, which now owns them.

## Objective

Final verification, closing out Story 03.0 and milestone 0009. All five
format paths (`text`/`xml`/`json`/`yaml`/`csv`) must work end-to-end on
real hardware, through the actual `smbios` CLI, not just through unit
tests against synthetic fixtures. The examples and doc samples from Task 03
must be green on every OS.

## Files

This task adds no new test source. It is the milestone's exit gate. Task 03
owns the example, doc-sample and documentation files.

## Success criteria

- [ ] `smbios --format <name>` produces valid, correctly structured output
      on real hardware for every one of `text`, `xml`, `json`, `yaml` and
      `csv`. This must hold across Windows, Linux and macOS (the native
      sources from milestone 0005).
- [ ] `text` output is confirmed byte-identical to the tool's behaviour
      before this milestone.
- [ ] The `smbios_cli_*` CTest entries from Task 02 pass. The two
      unknown-format tests must pass in CI on every OS.
- [ ] Every `smbios_example_*` CTest entry from Task 03 passes in CI on all
      three OSes, in both the `build` and `sanitizers` jobs.
- [ ] `DocSamplesTests` passes in `smbios_unit_test` on all three OSes, and
      `python3 scripts/check_doc_samples.py --check` passes in CI.
- [ ] Every Task 03 success criterion is checked, including the
      documentation and Doxygen rows.
- [ ] `/link-check docs/` (`python scripts/check_doc_links.py docs/`)
      reports zero problems across the full roadmap.
- [ ] Milestone 0009's `status.md` is updated to ✅ for all three stories.
- [ ] `docs/roadmap/README.md`'s milestone table and Program overview
      include milestone 0009.
