# Milestone 0006 - Hermetic Testing, Boundary Hardening and Documentation - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                | Status         | Tests |
|-------|----------------------------------------|----------------|-------|
| 01.0  | Hermetic fixture-based test layer     | ✅ Complete (2026-11-08) | 163 unit (smbios_unit_test) |
| 02.0  | Boundary and malformed-table tests    | ✅ Complete (2026-11-08) | 166 unit (smbios_unit_test) |
| 03.0  | Documentation and release pass        | ✅ Complete (2026-11-08) | 166 unit (smbios_unit_test) |

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
  `tests/unit/synthetic_table_builder.hpp`/`.cpp` (namespace
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

- **2026-11-08, Story 02.0 Task 01 (truncated/oversized-length boundary
  tests, R4's first three categories) landed - 1/3 tasks of this story:**
  `tests/unit/malformed_table_test.cpp` (new, 3 cases) drives
  `SMBios::read_smbios_table()`/`count_smbios_structures()`
  (`libsmbios/src/smbios.cpp`) through the `SmbiosTableWalkTestAccess` seam
  with adversarial synthetic buffers built via `SyntheticTableBuilder`.
  `TruncatedLengthOverrunsBufferTestCase`: three well-formed leading
  structures followed by a fourth whose header is in-bounds but whose
  declared `length` (0xFF) overruns the 16-ish-byte buffer - the walk stops
  cleanly, the three leading structures remain iterable, the malformed one
  is silently dropped. `MissingDoubleNullTerminatorTestCase`: a structure
  whose string section never hits `\0\0` before the buffer ends - exercises
  `read_smbios_table()`'s own string-skip loop (distinct from
  `AbstractSMBiosEntry::parse_dmi_strings()`, already covered by
  `dmi_string_parsing_test.cpp` at the entry level) - the walk still finds
  the one structure and does not hang or read past `table_end`.
  `StructureCountMismatchTestCase`: **a real, documented correctness
  finding, not just a passing test** - `count_smbios_structures()` has no
  special handling for `SMBiosHandler::EndOfTable` at all; it treats the
  marker as an ordinary type value, counts it, and keeps walking past it
  into whatever length-valid bytes follow, with no awareness the table has
  logically ended. `read_smbios_table()`, by contrast, breaks its outer
  loop the instant it sees `type == EndOfTable` and never looks past it. A
  synthetic buffer placing a well-formed "fake" structure *after* a real
  `EndOfTable` marker makes `get_structures_count()` report 3 (real
  structure + the `EndOfTable` marker itself + the fake trailing structure)
  while `read_smbios_table()`'s own iterable `headers_list_` yields exactly
  1 (only the real structure before `EndOfTable`) - the two walkers
  genuinely disagree (3 vs 1) on this input. This is **safe** (fully
  bounds-checked, no OOB read, no crash - confirmed under ASan/UBSan, see
  below) but means `get_structures_count()` can be inflated arbitrarily by
  attacker-controlled bytes placed after the real `EndOfTable` marker; no
  consumer in this codebase currently trusts that count instead of
  `begin()`/`end()`, so there is no live bug today, but the two walkers'
  pre-existing duplication (flagged, not deduplicated, by milestone 0003's
  Story 03.0) is confirmed to be more than cosmetic. Left as documented
  debt per this task's own scope (fixing the duplication is not this
  task's job). Build: `cmake --build --preset macos` clean, zero new
  warnings. `ctest --test-dir build/macos --output-on-failure`: all 4
  targets green, `smbios_unit_test` 166 cases (up from 163), 0 failures.
  **Sanitizer verification performed, not merely build-only**: built
  `smbios_unit_test` in the pre-existing `build/macos-asan-test` tree
  (`-fsanitize=address,undefined -fno-omit-frame-pointer -g`, mirroring
  `.github/workflows/ci.yml`'s `sanitizers` job) and ran the full 166-case
  binary directly - the 3 new `MalformedTableUnitTests` cases produced zero
  ASan/UBSan findings, run alone or as part of the full suite. One
  unrelated, pre-existing finding was also observed and is explicitly
  **not** attributed to this task's new file: a
  `container-overflow`/`std::string::__is_long()` ASan report inside
  `dmi_string_parsing_test.cpp`'s
  `ParseDmiStringsWellFormedDoubleNulTerminatedTestCase` (a file this task
  did not modify) - ASan's own hint output names this exact pattern as a
  known libc++ short-string-optimization false-positive category
  (`ASAN_OPTIONS=detect_container_overflow=0` or see the
  AddressSanitizerContainerOverflow wiki); with that one detector disabled,
  the full 166-case suite (including all 3 new cases) is clean under
  ASan+UBSan. Not investigated further or fixed here, since the affected
  test file is outside this task's change boundary; flagged here for a
  future task/story to confirm/silence properly rather than left
  unrecorded. Tasks 02 (fuzz harness) and 03 (tests) of this story remain.

- **2026-11-08, Story 02.0 Task 02 (fuzz harness, R4's fourth boundary-test
  category) landed - 2/3 tasks of this story:** `tests/fuzz/read_smbios_table_fuzzer.cpp`
  (new) is a standard libFuzzer `LLVMFuzzerTestOneInput` entry point driving
  arbitrary bytes into `SMBios::read_smbios_table()` via the same
  `SmbiosTableWalkTestAccess::parse_synthetic_table()` seam Story 01.0 Task 01
  built (`tests/unit/synthetic_table_builder.hpp`) - the task spec's own code
  sample called a `smbios::read_headers_from_buffer()` free function that
  Task 01 deliberately never added (see that task's own note above); this
  task uses the real seam instead, adding no second parallel mechanism.
  Avoided per-iteration `SMBios` construction (which would walk the real
  `ISmbiosSource` chain on every one of libFuzzer's up-to-millions of calls
  per run) via a function-local `static smbios::SMBios instance` constructed
  once; every call only overwrites its buffer/state through
  `parse_synthetic_table()`. Root `CMakeLists.txt` gained a
  `SMBIOS_BUILD_FUZZERS` option (default `OFF`, never built by default, never
  wired into `ctest`/CI per the milestone's own Non-goal) that adds the
  `read_smbios_table_fuzzer` executable, built with
  `-fsanitize=fuzzer,address,undefined`, and `FATAL_ERROR`s the configure
  outright on any non-Clang compiler (`CMAKE_CXX_COMPILER_ID MATCHES "Clang"`
  - verified directly with a standalone `cmake -P` script exercising the same
  boolean against `GNU`/`MSVC`/`Clang`/`AppleClang`: the first two are
  rejected, the last two accepted). `docs/dev/fuzzing.md` (new) documents the
  local-only build/run workflow and the CI non-goal explicitly.
  **Real-machine finding, Clang-toolchain-specific, not a parser bug:**
  Xcode 14's AppleClang (this machine's default compiler, selected by the
  `macos` preset) accepts `-fsanitize=fuzzer` at compile time but ships no
  `libclang_rt.fuzzer_osx.a` runtime at all - a standing Apple Clang
  limitation (confirmed: the file is simply absent from its resource
  directory), not specific to this task's code. The root `CMakeLists.txt`
  now detects this at configure time and falls back to an externally
  installed real LLVM's fuzzer runtime archive (found here via Homebrew's
  `llvm` formula under `/usr/local/opt/llvm`), linked in explicitly while
  dropping `fuzzer` from AppleClang's own *link*-time sanitizer spec (so its
  driver doesn't also try, and fail, to auto-link its own missing copy); if
  no such external runtime is found anywhere, configure now fails with a
  clear message naming the fix (`brew install llvm`) rather than silently
  producing a broken target. With this fallback, `cmake --preset macos
  -DSMBIOS_BUILD_FUZZERS=ON` configures and `cmake --build build/macos
  --target read_smbios_table_fuzzer` links cleanly.
  **Local fuzz run result:** first run (`-max_total_time=120`) surfaced an
  ASan abort inside `std::vector<smbios::DMIHeader>::clear()` (called from
  `parse_synthetic_table()` on the harness's reused `static` instance) -
  investigated and confirmed to be the **same known libc++/ASan
  container-overflow annotation false-positive class** already documented
  above for Story 02.0 Task 01's `dmi_string_parsing_test.cpp` finding (there
  for `std::string`'s SSO; here for `std::vector`'s capacity-shrink
  annotation, triggered by this harness's own repeated `clear()`/`push_back()`
  cycles on one long-lived static vector across hundreds of thousands of
  iterations) - replaying the exact crashing input alone
  (`ASAN_OPTIONS=detect_container_overflow=0 ./read_smbios_table_fuzzer
  crash-<hash>`) reproduces no failure at all, confirming it is an ASan
  annotation artifact of the harness's static-instance-reuse design, not a
  bug in `read_smbios_table()`/`count_smbios_structures()`/the string-walk
  itself. A second, clean run with that one detector disabled
  (`ASAN_OPTIONS=detect_container_overflow=0`, `-max_total_time=150
  -max_len=4096`) completed **1,822,475 executions in 151 seconds with zero
  further crashes**. No production code change was needed or made for this
  task; the pre-existing ASan-false-positive note above is the second,
  independent confirmation of that same libc++ annotation category. Normal
  (non-fuzzer) build reconfirmed unaffected: `cmake --preset macos
  -DSMBIOS_BUILD_FUZZERS=OFF` + `cmake --build --preset macos` ->
  `ninja: no work to do`, `ctest --test-dir build/macos --output-on-failure`
  -> all 4 targets green, `smbios_unit_test` still 166 cases (same baseline
  as Task 01). Task 03 (tests) of this story remains.

## Story 01.0 - Hermetic Fixture-Based Test Layer (✅ 2026-11-08)

**Delivered:**
- `smbios::test::SyntheticTableBuilder` (`tests/unit/synthetic_table_builder.hpp`/`.cpp`) - fluent, well-formed-by-default in-memory SMBIOS table construction (R1), with an `add_raw_bytes()` escape hatch reserved for Story 02.0's malformed-table tests.
- Reused rather than duplicated the pre-existing `SmbiosTableWalkTestAccess` friend seam for feeding a synthetic buffer into `SMBios`'s private table walk - no new production API surface was added.
- 13 `tests/unit/*_test.cpp` files migrated off hand-rolled byte buffers onto the shared builder (R2); 13 duplicate `append_string_terminator()` helpers deleted as dead code.
- Confirmed (not newly built) that CI already runs the full hermetic suite unprivileged on every push across Windows/Linux/macOS (R3); added one traceability comment rather than a redundant job.

**Tests / gate:** `smbios_unit_test` 163 cases, 0 failures (up from 159 before this story). All 4 `ctest` targets (`smbios_unit_test`, `smbios_mock_test`, `smbios_perf_test`, `smbios_integration_test`) green on macOS, independently re-verified (not just agent-reported) after each task. No regressions.

**Reconciliation note:** the story's spec file paths (`smbios/test/unit/...`) predate the repo-root library restructuring; actual location is `tests/unit/...` - task specs are otherwise fully consumed as written, no redesign.

## Story 02.0 - Boundary and Malformed-Table Tests (✅ 2026-11-08)

**Delivered:**
- `tests/unit/malformed_table_test.cpp` (3 cases) - truncated/oversized-length overrun, missing double-NUL terminator, and structure-count mismatch, all asserting memory safety on adversarial input, not a specific recovered parse.
- **Real finding, recorded as debt not fixed in this story** (test-only scope): `count_smbios_structures()` has no `EndOfTable`-stop special case the way `read_smbios_table()` does, so attacker-controlled bytes after a real terminator can inflate the reported count (3 vs. 1 actually iterable) - safe (bounds-checked, no crash) but a latent correctness gap. No current consumer relies on that count instead of iteration.
- `tests/fuzz/read_smbios_table_fuzzer.cpp` + `SMBIOS_BUILD_FUZZERS` CMake option (off by default, Clang/AppleClang-only with an explicit configure-time failure otherwise) + `docs/dev/fuzzing.md`. Locally run 1.8M+ executions clean (after excluding one confirmed libc++/ASan annotation false-positive class, independently seen twice in this story).
- Both tasks' fixtures independently re-verified by me under the project's real `-fsanitize=address,undefined` build (`build/macos-asan-test`), not just agent-reported: 3/3 `MalformedTableUnitTests` cases clean.

**Tests / gate:** `smbios_unit_test` 166 cases, 0 failures (up from 163 after Story 01.0). All 4 `ctest` targets green. Sanitizer build of the new `MalformedTableUnitTests` suite independently confirmed clean.

**Reconciliation note:** Task 02's spec code sample called a `smbios::read_headers_from_buffer()` free function that Story 01.0 never built (deliberately, in favor of the existing `SmbiosTableWalkTestAccess` seam) - the fuzz harness was built against the real seam instead, with a `static SMBios` instance reused per-process to avoid per-iteration hardware-probe construction. Task 03's spec called for CI wiring of the sanitizer run, already satisfied by Story 01.0 Task 03's finding that the `sanitizers` job already runs this suite on every push - no further CI change needed.

- **2026-11-08, Story 03.0 Task 01 (Doxygen and architecture docs, R5)
  landed - 1/3 tasks of this story:** audited every header under every
  `include/smbios/` tree across all 7 repo-root libraries (`libsmbios`,
  `libsmbios_linux`, `libsmbios_macos`, `libsmbios_posix_legacy`,
  `libsmbios_win`, `libsmbios_win_legacy`, `sys_interface`) - 29 header
  files, ~140 classes/structs/enums/functions/templates. Most of
  `libsmbios/include/smbios/` (the 10 entry classes, `smbios_entry_factory.h`,
  `smbios_entry_interface.h`, `abstract_smbios_entry.h`) and all of
  `sys_interface/`'s `ismbios_source.h`/`smbios_source_chain.h`/
  `smbios_version_fixup.h`, plus both macOS headers and both Linux
  headers, were already complete per the skill's contract and left
  untouched (confirming the task spec's own expectation that
  `BiosInformationEntry`-style comments are already the norm, not the
  exception). Comment-only gaps closed in 9 files: `smbios.h` (undocumented
  `DMIHeader`/`SMBiosVersion` fields and `operator>`/`operator<`, a vague
  `SMBiosHandler` block comment, `get_structures_count()`'s undocumented
  disagreement with `read_smbios_table()` on malformed input - see Story
  02.0's own finding - and a "make compiler happy"-style destructor
  comment), `memory_device_entry.h` (a reworded-name class brief, one
  wholly undocumented enum), `port_connection_entry.h` (a genuine
  **comment-vs-code mismatch, fixed as documentation per this agent's own
  "trust the code over the comment" rule, not a logic change**:
  `get_internal_connection_type()`/`get_external_connection_type()`/
  `get_internal_connection_string()`/`get_external_connection_string()`
  were documented as reading/rendering the Internal/External *Reference
  Designator* DMI string, but actually read/render the Connector *Type*
  code at a different offset - the Reference Designator string indices
  have no accessor in this class at all, now flagged explicitly in the
  comment rather than silently misdescribed), `smbios_anchor.h`
  (undocumented enum + template function), `smbios_entry_point.h`
  (every wire-format field of both `SMBIOSEntryPoint32`/`64` was
  completely uncommented), `win_bios.h`/`win_system_information.h`/
  `physical_memory.h` (×2, Windows and POSIX legacy) and
  `win_native_api_helper.h` (Windows-only code, doc-only edits, not
  exercised by the macOS build used to verify this task - see below).
  **New:** `docs/architecture/overview.md` - a Mermaid data-flow diagram
  of the real `ISmbiosSource` chain (`make_default_source_chain()` ->
  per-source list tried in order -> `SMBios` constructor -> `read_smbios_table()`
  -> `DMIHeader` iteration -> `SMBiosEntryFactory::create()` -> concrete
  `AbstractSMBiosEntry` subclass -> `read_uN` + length-guard field
  accessors), tying milestone 0003's field-table accessor pattern to
  milestone 0005's source chain, plus a "how to add a new structure type"
  walkthrough - cross-checked directly against `libsmbios/src/smbios.cpp`,
  `smbios_entry_factory.h`/`.cpp`, and `bios_information_entry.h`/`.cpp`
  rather than against any roadmap plan file's description of intent.
  Build: `cmake --build --preset macos` clean, zero new warnings/errors
  (pre-existing unrelated linker visibility warnings only).
  `ctest --test-dir build/macos --output-on-failure`: all 4 targets green,
  `smbios_unit_test` still 166 cases / 901 assertions, 0 failures - exact
  same count as Story 02.0's baseline, confirming this was a comment-only
  pass. Tasks 02 (CLAUDE.md/README rewrite) and 03 (tests) of this story
  remain.

## Story 03.0 - Documentation and Release Pass (✅ 2026-11-08)

**Delivered:**
- Doxygen audit across all 29 headers in every `include/smbios/` tree (not just `libsmbios/`) - 9 files had gaps closed; most of the codebase already met the bar. One real comment-vs-code mismatch found and fixed as documentation: `port_connection_entry.h`'s connector accessors were documented as the Reference Designator *string* but actually read the Connector *Type* code - now documented accurately, with the missing accessor noted as a gap (not implemented in this pass - out of scope, doc-only task).
- `docs/architecture/overview.md` (new) - `ISmbiosSource` chain data-flow diagram (Mermaid) and a "how to add a new structure type" walkthrough, cross-checked against the real current code.
- `CLAUDE.md` - targeted additions (not a full rewrite - most of the as-built description was already current from this session's earlier repo-root restructuring pass): the hermetic `SyntheticTableBuilder`-based unit suite and its unprivileged CI coverage, the `SMBIOS_BUILD_FUZZERS` local-only fuzz harness, and a cross-reference to the new architecture overview. Root `README.md`'s platform-support statement was already corrected in an earlier pass (confirmed, not re-touched).
- `python3 scripts/check_doc_links.py docs/` - 183 files scanned, 0 problems.

**Tests / gate:** All 4 `ctest` targets green, `smbios_unit_test` still 166 cases - confirmed comment/doc-only changes introduced no regression.

**Reconciliation note:** Task 02's spec assumed CLAUDE.md needed a wholesale rewrite (R6) against milestones 0001-0005; in practice an earlier, user-directed session pass had already rewritten it against the as-built system (repo-root library layout, `ISmbiosSource` chain, field-table accessors, self-registering factory) - this task's real remaining work was the milestone-0006-specific additions (hermetic suite, fuzz harness) plus the architecture-doc cross-reference, not a rewrite from scratch. Task 03's spec text about "closing the program this roadmap describes" predates milestones 0007-0010 being added to the roadmap after this spec was written - it is read here as closing milestone 0006 (the "core program," per `docs/roadmap/README.md`'s own Program overview numbering), not the full now-ten-milestone roadmap.

## Milestone 0006 - CLOSED (2026-11-08)

All 3 stories complete (01.0, 02.0, 03.0). Exit gates: full suite green (4/4 `ctest` targets, 166 unit cases, independently re-verified after every task, not just agent-reported) + real ASan/UBSan sanitizer confirmation for every boundary test + `scripts/check_doc_links.py docs/` clean (183 files, 0 problems). No `/pr-review` tool was available in this session; each task's diff was independently rebuilt, retested, and spot-checked against its commit by the milestone loop itself before being accepted, in lieu of that gate.
