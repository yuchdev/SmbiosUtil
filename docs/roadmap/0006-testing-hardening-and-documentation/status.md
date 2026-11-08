# Milestone 0006 - Hermetic Testing, Boundary Hardening and Documentation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                | Status         | Tests |
|-------|----------------------------------------|----------------|-------|
| 01.0  | Hermetic fixture-based test layer     | ✅ Complete (2026-11-08) | 163 unit (smbios_unit_test) |
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
- **2026-11-08, Story 01.0 Task 01 (`SyntheticTableBuilder`) landed:**
  `tests/unit/synthetic_table_builder.h`/`.cpp` (namespace
  `smbios::test`), reusing the existing `SmbiosTableWalkTestAccess` friend
  seam (moved there from `smbios_table_walk_test.cpp`, which now includes
  the shared header instead of duplicating it) rather than adding the
  task spec's originally-proposed `read_headers_from_buffer()` free
  function or a new `SMBios` constructor overload - the seam already
  satisfied the "no `ISmbiosSource` involved" constraint, so a second
  parallel mechanism was unnecessary. `add_structure()` auto-computes
  `length` and auto-assigns sequential handles (not a builder parameter,
  per the task's own shown interface); `add_raw_bytes()` suppresses the
  automatic `EndOfTable` append for Story 02.0's future malformed-table
  tests. Self-tested in `tests/unit/synthetic_table_builder_test.cpp`
  (4 new cases). Full suite: 163 `smbios_unit_test` cases pass (was 159),
  all 4 `ctest` targets green. Task 02 (migrating
  `build_port_connection_buffer()` and friends onto the builder) and
  Task 03 remain.
- **2026-11-08, Story 01.0 Task 02 (migrate ad hoc buffers onto
  `SyntheticTableBuilder`) landed:** every hand-rolled synthetic-buffer
  construction under `tests/unit/` that assembled a well-formed DMI
  structure or table by hand (header push_backs + manual double-NUL
  string terminator) now goes through
  `smbios::test::SyntheticTableBuilder::add_structure()`/`build()` instead
  - 13 files: the 10 `*_entry_test.cpp` field-table test files
  (`bios_information`, `memory_device`, `port_connection`,
  `system_information` (both its own suite and its UUID-formatting
  fixture), `system_enclosure`, `processor_information`,
  `cache_information`, `system_slots`, `physical_memory_array`,
  `memory_array_mapped_address`, `system_boot_information`),
  `dmi_string_parsing_test.cpp` (including its deliberately-malformed
  missing-terminator case, now via `add_raw_bytes()`),
  `smbios_table_walk_test.cpp`'s own two multi-structure literals, and
  `unit_tests.cpp`'s `build_port_connection_buffer()` (named explicitly by
  this task despite predating the spec's own file list). Per-file
  `append_string_terminator()` duplicate helpers (13 near-identical
  copies) are gone; `DMIHeader`-from-buffer construction (`make_header()`)
  is unchanged, since that is unaffected by how the underlying buffer was
  assembled. No assertion/expected-value changed - every migrated test's
  own byte layout was re-derived from the builder's `add_structure()`
  (header + formatted area verbatim + auto-terminated strings), which
  matches the original hand-rolled layout byte-for-byte, so no bug was
  exposed. Left alone (genuinely out of scope, not full-table/structure
  construction): `linux_sysfs_source_test.cpp`,
  `smbios_source_chain_test.cpp` (fake `ISmbiosSource` payload bytes, not
  DMI structures), `byte_reader_test.cpp`,
  `smbios_entry_factory_test.cpp`'s/`smbios_version_fixup_test.cpp`'s/
  `smbios_legacy_link_test.cpp`'s/`smbios_source_chain_link_seam_test.cpp`'s
  narrow non-table byte usage, and the builder's own
  `synthetic_table_builder.{h,cpp,_test.cpp}`. Full suite: still 163
  `smbios_unit_test` cases pass (same count as the Task 01 baseline), all
  4 `ctest` targets green. The spec's own proxy grep
  (`std::vector<uint8_t>.*{0x`) only moves 9 -> 10 (it was never a
  reliable signal for this codebase's push_back-style construction); the
  real signal is `grep -rl synthetic_table_builder.h tests/unit/*.cpp`:
  3 files before this task, 16 after. Task 03 remains.
- **2026-11-08, Story 01.0 Task 03 (CI wiring, R3) landed - Story 01.0
  complete:** found that the full hermetic suite was *already* running
  unprivileged on every push for all three OSes, just not under a job
  named `hermetic-tests` as the stale task spec assumed - Windows via
  the `build` job's existing "Run unprivileged tests" step, Linux/macOS
  via the `sanitizers` job's existing "Run unprivileged tests
  (smbios_unit_test, smbios_mock_test)" step. Added a single traceability
  comment at the top of `.github/workflows/ci.yml` naming which step
  covers which OS and why the root/admin-needing "functional test"/
  "integration test" steps are the privileged complement, not a
  duplicate, rather than introducing a fourth near-identical job - a new
  job would not have made the suite any more unprivileged or any more
  complete than the jobs already doing this work, only added
  maintenance surface. YAML syntax validated (`python3 -c "import
  yaml; yaml.safe_load(...)"`). Full suite re-confirmed green: 163
  `smbios_unit_test` cases, all 4 `ctest` targets pass.

## Story 01.0 - Hermetic Fixture-Based Test Layer (✅ 2026-11-08)

**Delivered:**
- `smbios::test::SyntheticTableBuilder` (`tests/unit/synthetic_table_builder.h`/`.cpp`) - fluent, well-formed-by-default in-memory SMBIOS table construction (R1), with an `add_raw_bytes()` escape hatch reserved for Story 02.0's malformed-table tests.
- Reused rather than duplicated the pre-existing `SmbiosTableWalkTestAccess` friend seam for feeding a synthetic buffer into `SMBios`'s private table walk - no new production API surface was added.
- 13 `tests/unit/*_test.cpp` files migrated off hand-rolled byte buffers onto the shared builder (R2); 13 duplicate `append_string_terminator()` helpers deleted as dead code.
- Confirmed (not newly built) that CI already runs the full hermetic suite unprivileged on every push across Windows/Linux/macOS (R3); added one traceability comment rather than a redundant job.

**Tests / gate:** `smbios_unit_test` 163 cases, 0 failures (up from 159 before this story). All 4 `ctest` targets (`smbios_unit_test`, `smbios_mock_test`, `smbios_perf_test`, `smbios_integration_test`) green on macOS, independently re-verified (not just agent-reported) after each task. No regressions.

**Reconciliation note:** the story's spec file paths (`smbios/test/unit/...`) predate the repo-root library restructuring; actual location is `tests/unit/...` - task specs are otherwise fully consumed as written, no redesign.
