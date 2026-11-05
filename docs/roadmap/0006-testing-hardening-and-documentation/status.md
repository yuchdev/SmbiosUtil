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
- **2026-11-05, ahead-of-schedule partial start, outside the roadmap's own
  sequencing (user-directed, not run via `implement-milestone`):** the
  test tree was already reclassified into `tests/{unit,mock,perf,integration}/`
  (Boost.Test, top-level, `enable_testing()` now actually called - it
  never was before), with a real `tests/unit/unit_tests.cpp` covering
  `SMBiosVersion` comparison (bug fixed in the same pass - lexicographic,
  matching milestone 0002's already-recorded defect #1) and
  `PortConnectionEntry` parsing from a hand-built buffer, plus an honest,
  documented `tests/mock/` placeholder (no injectable seam exists before
  this milestone's own Story 01.0 / milestone 0005's `ISmbiosSource`).
  `.claude/agents/testing-expert.md` and `docs/dev/cpp_coding_standard.md`
  were updated to describe this as the project's real, current scheme
  (Boost.Test, unit/mock/perf/integration - not GoogleTest or `e2e`).
  **Reconciliation for this milestone's own Story 01.0:** its planned
  `SyntheticTableBuilder` should *generalize* `build_port_connection_buffer()`'s
  pattern (already proven working) rather than duplicate or ignore it;
  `read_headers_from_buffer()` (Task 01 of that story) still needs
  building - this ad hoc work only added one entry type's worth of
  coverage via direct `AbstractSMBiosEntry` subclass construction, not a
  `SMBios`-level buffer-input path.
