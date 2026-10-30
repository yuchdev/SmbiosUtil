# 04 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run the three entry types' synthetic-buffer and functional tests together,
confirm `grep` shows zero remaining `*V2x`/`*V3x` struct definitions across
`smbios/include/smbios/`, and close out Story 02.0.

## Files

No new test source beyond Tasks 01-03's new test files - this is the
story's exit gate.

## Success criteria

- [ ] `grep -rn "pragma pack" smbios/include/smbios/bios_information_entry.h
      smbios/include/smbios/memory_device_entry.h
      smbios/include/smbios/port_connection_entry.h` returns no matches
      (every `#pragma pack` struct in these three headers is gone).
- [ ] All three entry types' synthetic-buffer tests pass.
- [ ] Full existing functional suite passes unmodified on real hardware.
- [ ] Milestone 0003's `status.md` Story 02.0 row is updated to ✅.
