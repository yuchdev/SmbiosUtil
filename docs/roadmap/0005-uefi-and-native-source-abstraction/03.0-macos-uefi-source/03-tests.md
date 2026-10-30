# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Confirm the macOS source works across whatever real Mac hardware is
available for testing (Apple Silicon and Intel, if both are reachable),
and close out Story 03.0.

## Files

No new test source beyond Task 02's addition - this is the story's exit
gate, plus a manual cross-architecture check if more than one Mac is
available.

## Success criteria

- [ ] `smbios_util` runs successfully on at least one real Mac, unprivileged.
- [ ] If both Apple Silicon and Intel hardware are available, both are
      checked and any architecture-specific difference is recorded in
      `status.md`.
- [ ] Milestone 0005's `status.md` Story 03.0 row is updated to ✅.
