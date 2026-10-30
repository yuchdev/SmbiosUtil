# Milestone 0001 - Portable Build System and Dependencies - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                | Status         | Tests |
|-------|--------------------------------------|----------------|-------|
| 01.0  | vcpkg dependency migration           | ⬜ Not started | -     |
| 02.0  | CMake presets, target hygiene & CI   | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This milestone was scoped from a request to make the build "fully
  portable" and replace "manually added dependencies" with vcpkg, ahead of
  the C++20 modernization (0002), the parser redesign (0003), and the new
  UEFI sources (0005) that follow it. Build-system-first ordering was chosen
  so every later milestone's CI runs against the same build from day one,
  instead of retrofitting vcpkg once source changes are already underway.
