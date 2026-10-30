# Milestone 0002 - C++20 Modernization and Latent Bug Remediation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                          | Status         | Tests |
|-------|-------------------------------------------------|----------------|-------|
| 01.0  | Language standard raise and compiler hygiene   | ⬜ Not started | -     |
| 02.0  | Latent bug remediation                          | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Depends on milestone 0001 (vcpkg + CMake presets) being complete, so
  every fix in this milestone can be verified on Windows/Linux/macOS
  through the same build path.
- The four defects this milestone targets (`SMBiosVersion` comparison,
  `unix_bios.cpp`'s missing `return` in `get_table_size()`, the dishonestly
  "supported" `*nix` stubs, and the dead `// DEBUG` include) were identified
  by direct code review of `smbios/src/smbios.cpp` and
  `smbios/src/unix_bios.cpp`, not from a bug tracker - there is no external
  issue to cross-reference.
