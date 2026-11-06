# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Confirm the new library graph (contract **C3**) builds correctly on every
platform and introduces no behavior change, and stand up the first unit
test that is only possible *because* this story's library boundary now
exists - closing part of `tests/mock/`'s documented
("`SmbiosMockTests::NoMockableSeamYetTestCase`") placeholder gap ahead of
Story 02.0/03.0/04.0 landing their real sources, per
[docs/test/test-subsystem.md](/docs/test/test-subsystem.md)'s own stated
plan for this milestone.

## What is testable here, and what is not

- **Unit, no privileges needed:** `smbios_source_legacy` is now an
  independently-linkable target. A new
  `tests/unit/smbios_source_legacy_link_test.cpp` links
  `smbios_source_legacy` **alone** (not `smbios`, not any per-OS library)
  and constructs a `LegacyMemoryScanSource`, calling `is_available()`
  (expected `true`, unconditionally, per Story 01.0 Task 02) and
  `source_name()` (expected `"legacy-scan"` or whatever literal Story
  01.0 settles on) - proving DD2/DD3 from ADR 0001 in code, not just on
  paper: this library has no dependency on any per-OS library and compiles
  standalone.
- **Unit, no privileges needed:** a new
  `tests/unit/smbios_source_chain_link_seam_test.cpp` confirms that
  linking `smbios` (core) alone is sufficient to resolve
  `smbios::make_default_source_chain()` with no unresolved symbol - the
  concrete proof that this story's "link-seam instead of preprocessor-seam"
  resolution of R8 actually works, on whichever platform CI runs this
  suite on.
- **Mock seam, no privileges needed, but genuinely new:** this story does
  not yet have a real `ISmbiosSource` fake to substitute (Stories
  02.0/03.0/04.0 own the real implementations), so `tests/mock/`'s
  placeholder test case is **not** removed here - but its own comment
  should be updated to note that the injectable seam (`ISmbiosSource`)
  and the library boundary it now lives behind both exist as of this
  story, and the placeholder's removal is now blocked only on a concrete
  fake source, not on any remaining architectural gap. Record this in
  `docs/test/test-subsystem.md` as well (its "Intended coverage" section
  already names milestone 0005 for this; add a one-line pointer to this
  story specifically).
- **Cannot be CI-verified here, same honest limitation named by Stories
  02.0/03.0/04.0's own test tasks:** that `smbios_source_windows` produces
  identical real output on real Windows hardware, and likewise for Linux/
  macOS, is exactly what the existing functional/integration suite
  already checks (unchanged by this story) - this task does not add a new
  real-hardware check beyond confirming those existing suites still pass
  after the relocation.

## Files

- `tests/unit/smbios_source_legacy_link_test.cpp` (new).
- `tests/unit/smbios_source_chain_link_seam_test.cpp` (new).
- `docs/test/test-subsystem.md` - one-line addition noting this story as
  the point where the mock-seam's remaining blocker became "needs a fake
  `ISmbiosSource`," not "needs an injectable interface to exist at all."

## Constraints

- Neither new test links more than one per-OS library - if a test needs
  to exercise `smbios`'s actual resolved chain end-to-end on the current
  CI runner's OS, it links `smbios` (core) only, the same way
  `tests/integration/` already does, not an individual per-OS library
  directly (that would defeat the point of proving the link-seam resolves
  through the real target graph).
- Existing `tests/unit/`, `tests/mock/`, `tests/perf/`,
  `tests/integration/` CMakeLists.txt files are not restructured - the
  two new unit test files are added to `tests/unit/CMakeLists.txt`'s
  existing `file(GLOB SOURCES *.cpp)` with no new `target_link_libraries`
  entries needed for most of `tests/unit/`'s suite (it still links
  `smbios` only), **except** the new `smbios_source_legacy_link_test.cpp`,
  which needs its own `target_link_libraries(smbios_unit_test PRIVATE
  smbios_source_legacy)` addition - the one real CMake change this task
  makes outside the new library directories themselves.

## Success criteria

- [ ] `smbios_source_legacy_link_test` passes on every platform, proving
      `smbios_source_legacy` has zero dependency on any per-OS library.
- [ ] `smbios_source_chain_link_seam_test` passes on every platform,
      proving `smbios::make_default_source_chain()` resolves through
      `smbios` core's link graph with no unresolved symbol.
- [ ] Full existing unit/mock/perf/integration suites pass unmodified on
      Windows, Linux, and macOS (perf/integration: wherever real hardware
      and privileges are available, same as every other story in this
      milestone).
- [ ] `render_to_description()`'s output is byte-identical before/after
      this story on each platform (same bar Story 01.0's own Task 03
      already set - this story must not regress it a second time).
- [ ] Milestone 0005's `status.md` Story 01.1 row is updated to ✅.
