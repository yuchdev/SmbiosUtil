# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the Linux chain (`sysfs → EFI → legacy scan`) behaves correctly
end-to-end on real hardware/VMs covering each availability combination,
closing out Story 02.0.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate, plus one integration check:

- Manually run (documented, not CI-automated, since it needs root and
  real `/sys`/`/dev/mem` access) on at least: (a) a Linux VM/machine with
  readable sysfs DMI tables, (b) one with only EFI systab available
  (sysfs DMI tables absent or unreadable), (c) one with neither (forces
  legacy-scan fallback) - confirming `source_name()` reports the expected
  source in each case and the parsed table content agrees across whichever
  of (a)/(b)/(c) are simultaneously available on the same test machine.

## Success criteria

- [ ] All three Linux availability combinations produce correct,
      mutually-consistent output where cross-checkable.
- [ ] `grep -n "return false;" smbios/src/unix_bios.cpp` (or wherever
      `sysfs_table_exists()`'s old body lived, if `unix_bios.cpp` is
      retired per the story's Key constraints) no longer represents a
      permanently-stubbed Linux source.
- [ ] Milestone 0005's `status.md` Story 02.0 row is updated to ✅.
