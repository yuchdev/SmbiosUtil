# Test Subsystem

What this project actually tests today, grouped by suite - and what the
roadmap commits to testing next. Each group below names its **type**, what
it **covers**, and **why it matters** (the failure class it guards
against). For exact assertions, read the suite itself; this document stops
at the group level on purpose.

## Taxonomy

Four categories, each its own top-level directory and its own Boost.Test
executable, all wired into `ctest` via `enable_testing()` in the root
`CMakeLists.txt`:

| Category    | Directory             | Target                     | Needs root/admin? | Always expected green? |
|--------------|------------------------|------------------------------|--------------------|--------------------------|
| Unit         | `tests/unit/`          | `smbios_unit_test`          | No                 | Yes |
| Mock         | `tests/mock/`          | `smbios_mock_test`          | No                 | Yes (currently a documented placeholder - see below) |
| Perf         | `tests/perf/`          | `smbios_perf_test`          | Yes (real hardware) | No - fails cleanly without privileges, not a regression |
| Integration  | `tests/integration/`  | `smbios_integration_test`  | Yes (real hardware) | No - same as Perf |

**No `e2e` category.** A hardware-inventory library/CLI has no user journey
distinct from "construct an `SMBios`, read its structures" - that journey
*is* the integration suite. Adding a separate `e2e` layer here would just
duplicate it under a different name.

Framework is Boost.Test (`BOOST_AUTO_TEST_SUITE`/`BOOST_AUTO_TEST_CASE`)
throughout, matching the vcpkg-declared `boost-test` dependency. Run one
suite directly for a focused check: `./smbios_unit_test
--run_test=<SuiteName> --log_level=message`; run everything via `ctest
--test-dir build/<preset> --output-on-failure`.

## Implemented coverage

### Unit - `tests/unit/` (`smbios_unit_test`)

Deterministic, hand-built byte buffers and `DMIHeader`s - no real hardware,
no privileges, always expected to pass. This is where every parsing rule
and every regression fix lands first.

| Suite | Covers | Guards against |
|-------|--------|------------------|
| `SmbiosUnitTests` (`unit_tests.cpp`) | `SMBiosVersion::operator>`/`operator<` lexicographic comparison; `PortConnectionEntry` parsing from a synthetic buffer; `SMBiosEntryFactory`'s type→class lookup (including the unregistered-type `nullptr` case); `SMBiosImpl::get_table_size()` against an injected buffer | The historical version-comparison bug (`{2,9} > {3,0}` evaluating wrong) silently corrupting which versioned fields a caller thinks are present; the factory returning the wrong entry class or crashing on an unknown SMBIOS type; a table-size regression (the milestone 0002 missing-`return` defect) |
| `ByteReaderUnitTests` (`byte_reader_test.cpp`) | `smbios::read_u8/u16/u32/u64` - little-endian byte composition, `constexpr`-verified via `static_assert` | A naive `reinterpret_cast`-style read silently reading multi-byte SMBIOS fields in host byte order instead of the DMTF spec's little-endian wire format - the exact class of bug milestone 0003 exists to eliminate |
| `BiosInformationEntryUnitTests` (`bios_information_entry_test.cpp`) | Every `BiosInformationEntry` field accessor across the three real length tiers (`< 0x12`, `0x12`-`0x18`, `> 0x18`), including the newly-added `get_extended_rom_size()` | A length-guard regression silently reading past a truncated structure's declared bounds, or a field decoding at the wrong byte offset (two such offset bugs were found and fixed by this exact suite's sibling, see Memory Device below) |
| `MemoryDeviceEntryUnitTests` (`memory_device_entry_test.cpp`) | Every `MemoryDeviceEntry` field accessor across all five legacy length tiers, including the five previously-unexposed getters (`get_extended_size()`, `get_memory_clock_speed()`, three voltage fields) | The real latent out-of-bounds read closed in milestone 0003 (the old `MemoryDeviceV23` pointer-overlay technique could read 4 trailing fields past a structure's actual length for `0x17 <= length < 0x1B`) and two doc-comment offset bugs (`get_array_handle()`, `get_device_rank()`) this suite caught by exercising hand-computed expected values at each tier |
| `SmbiosTableWalkUnitTests` (`smbios_table_walk_test.cpp`) | `SMBios::read_smbios_table()`/`count_smbios_structures()`'s bounds checks against a truncated last structure, an exact-boundary-fitting header, and an empty buffer | An out-of-bounds read past the mapped SMBIOS table on a malformed/truncated input - confirmed via a real pre-fix/post-fix behavior difference, with AddressSanitizer (the CI `sanitizers` job) as the independent proof this suite's assertions alone can't fully provide locally |

### Mock - `tests/mock/` (`smbios_mock_test`)

**Intentional placeholder.** `SmbiosMockTests::NoMockableSeamYetTestCase`
exists only to document *why* this suite has no real cases yet and to keep
Boost.Test from treating a zero-case suite as a setup error - it makes no
assertion about production behavior. `SMBios` and `PhysicalMemory` both
select their platform source via a compile-time `#if`, not an injectable
interface, so there is no boundary to substitute a test double at. This
closes the moment milestone 0005's `ISmbiosSource` interface lands - see
**Intended coverage** below.

### Perf - `tests/perf/` (`smbios_perf_test`)

`SmbiosPerformanceTests::PerformanceTestsCase` times a real `SMBios`
construction and full-table enumeration against the host's live firmware
table, logging per-structure timing via `BOOST_TEST_MESSAGE`. Needs
root/admin on most platforms (the same privilege the native source itself
needs) and fails cleanly, not catastrophically, without it. Guards against
a parsing-path regression that is correct but silently pathological (e.g.
an accidental O(n²) re-scan per structure) - a defect class unit tests
cannot see since they never run against a realistically-sized real table.

### Integration - `tests/integration/` (`smbios_integration_test`)

`SmbiosFunctionalTests` constructs a real `SMBios` against the live
machine's actual firmware table (`SMBiosCreationTestCase`:
version/structure-count/table-base/table-size are all non-empty;
`SMBiosIterationTestCase`: every yielded header is non-empty) and factory-
creates a real entry object per header. Needs root/admin and fails cleanly
without it, same as Perf. This is the suite that proves the whole stack -
native source discovery, legacy-scan fallback, table walk, entry
factory - actually produces a sane result end-to-end on a real host,
which no synthetic-buffer unit test can claim regardless of how many
tiers it covers.

## Intended coverage (committed, not yet implemented)

Each item below is a specific roadmap commitment, not a vague aspiration -
linked to the milestone/story that owns it.

- **Milestone 0003, Story 03.0, Task 02** - bounds-checked string-section
  parsing tests (`AbstractSMBiosEntry::parse_dmi_strings()` against a table
  missing its terminating double-null marker), the string-side counterpart
  to the table-walk tests already landed above.
- **Milestone 0004** - a unit suite per newly-implemented SMBIOS structure
  type (the factory's expanded coverage beyond today's three entry
  classes), following the exact length-tier pattern `BiosInformationEntry`/
  `MemoryDeviceEntry` already established.
- **Milestone 0005** - the real `SmbiosMockTests` suite: once `SMBios`
  depends on the `ISmbiosSource` interface instead of a compile-time `#if`,
  this is where a fake source gets substituted to assert chain/fallback
  behavior (e.g. "falls through to the next source when `is_available()`
  is false") without needing real hardware or root.
- **Milestone 0006, Story 01.0** - a hermetic `SyntheticTableBuilder`
  (in-memory, well-formed SMBIOS table construction bypassing every
  `ISmbiosSource`), with every ad hoc synthetic buffer this document's
  existing unit suites hand-roll today migrated onto it, plus a CI job
  running the full hermetic suite unprivileged on every push - closing the
  gap where Perf/Integration can only prove the code *builds*, not that it
  *behaves correctly*, on a CI runner.
- **Milestone 0006, Story 02.0** - dedicated boundary/malformed-table
  tests beyond what Story 03.0 above covers: a structure count that
  disagrees with how many structures are actually found before
  `EndOfTable`, and a documented, locally-runnable fuzz harness feeding
  arbitrary bytes into `read_smbios_table()` (not run continuously in CI -
  a non-goal for that milestone).
