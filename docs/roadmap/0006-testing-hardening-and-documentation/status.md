# Milestone 0006 - Hermetic Testing, Boundary Hardening and Documentation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                | Status         | Tests |
|-------|----------------------------------------|----------------|-------|
| 01.0  | Hermetic fixture-based test layer     | ⬜ Not started | -     |
| 02.0  | Boundary and malformed-table tests    | ⬜ Not started | -     |
| 03.0  | Documentation and release pass        | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This milestone closes the program. It depends on every prior milestone
  (0001-0005) being complete, since Story 01.0's `SyntheticTableBuilder`
  retroactively replaces the ad hoc synthetic buffers several task specs
  in milestones 0003/0004 described inline, and Story 03.0 documents the
  system as it exists only once it has stopped changing.
- Several earlier task specs (e.g. milestone 0003's Story 01.0 Task 03,
  milestone 0005's Story 02.0 Task 01) deferred "a real CI-friendly,
  unprivileged test" to this milestone by name - this is that milestone.
