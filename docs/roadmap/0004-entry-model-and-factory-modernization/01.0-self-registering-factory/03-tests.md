# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the self-registering factory is fully equivalent to the old
hand-wired one before Story 02.0 starts adding new entry types on top of
it, closing out Story 01.0.

## Files

No new test source beyond Tasks 01-02's additions - this is the story's
exit gate.

## Success criteria

- [ ] `smbios_entry_factory_test.cpp`'s fake-entry and duplicate-registration
      cases pass.
- [ ] All three migrated entry types resolve correctly through `create()`.
- [ ] `boost-functional`/`boost-bind` are either removed from `vcpkg.json`
      or confirmed still needed elsewhere, with the reason recorded in
      `status.md`.
- [ ] Milestone 0004's `status.md` Story 01.0 row is updated to ✅.
