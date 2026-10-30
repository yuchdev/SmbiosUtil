# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run all eight new entry types' tests together, confirm
`SMBiosEntryFactory::create()` now returns a non-`nullptr` result for
every member of `SMBios::SMBiosHandler` except `EndOfTable` (which is
never constructed as an entry - it is the table's own terminator), and
close out milestone 0004.

## Files

No new test source beyond Tasks 01-03's additions - this is the story's
(and milestone's) exit gate.

## Success criteria

- [ ] Every `SMBiosHandler` value except `EndOfTable` resolves to a
      concrete, non-null entry via the factory.
- [ ] All eight new entry types' synthetic-buffer tests pass.
- [ ] `smbios_util` run against real hardware (CLAUDE.md's documented
      `--run_test=SmbiosFunctionalTests/SMBiosCreationTestCase` plus a
      manual CLI run) prints a complete report covering every structure
      type present on that machine, not just the original three.
- [ ] Milestone 0004's `status.md` is updated to ✅ for both stories.
