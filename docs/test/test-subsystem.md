# Test Subsystem

What this project actually tests today, grouped by suite - and what the
roadmap commits to testing next. Each group below names its **type**, what
it **covers**, and **why it matters** (the failure class it guards
against). For exact assertions, read the suite itself; this document stops
at the group level on purpose.

## Taxonomy

Four Boost.Test categories plus one plain-executable category (C ABI,
milestone 0007), each its own top-level directory, all wired into
`ctest` via `enable_testing()` in the root `CMakeLists.txt`:

| Category    | Directory             | Target                     | Needs root/admin? | Always expected green? |
|--------------|------------------------|------------------------------|--------------------|--------------------------|
| Unit         | `tests/unit/`          | `smbios_unit_test`          | No                 | Yes |
| Mock         | `tests/mock/`          | `smbios_mock_test`          | No                 | Yes (currently a documented placeholder - see below) |
| Perf         | `tests/perf/`          | `smbios_perf_test`          | Yes (real hardware) | No - fails cleanly without privileges, not a regression |
| Integration  | `tests/integration/`  | `smbios_integration_test`  | Yes (real hardware) | No - same as Perf |
| C ABI        | `tests/c_abi/`         | `smbios_c_compile_as_c_test`, `smbios_c_example`, `smbios_c_open_close_test`, `smbios_c_entry_iteration_test`, `smbios_c_typed_getters_test` | No (reads real hardware, but no elevated privilege needed - same as the native sources `smbios_c` sits on top of) | Yes - each checks `smbios_open()`'s own return code and degrades gracefully rather than failing when no table is available |

**GUI (`smbios_gui`, milestone 0011).** Its ImGui-free view-model layer (`libsmbios_gui_model`) is unit-tested in `tests/unit/gui_*_test.cpp`; the window, rendering and clipboard cannot be driven headlessly and are covered by the manual plan in [manual-testing.md](manual-testing.md).

**No `e2e` category.** A hardware-inventory library/CLI has no user journey
distinct from "construct an `SMBios`, read its structures" - that journey
*is* the integration suite. Adding a separate `e2e` layer here would just
duplicate it under a different name.

Framework is Boost.Test (`BOOST_AUTO_TEST_SUITE`/`BOOST_AUTO_TEST_CASE`)
throughout, matching the vcpkg-declared `boost-test` dependency. Run one
suite directly for a focused check: `./smbios_unit_test
--run_test=<SuiteName> --log_level=message`; run everything via `ctest
--test-dir build/<preset> --output-on-failure`.

## Keeping this registry current

This document is the registry of automated tests. **Whenever an automated test
is added, renamed, split, removed or moved to another category, update this
file in the same change**: add or adjust the suite's row under *Implemented
coverage* (suite name, file, case count, what it covers, what it guards
against), move any matching item out of *Intended coverage*, and update the
taxonomy table if a new executable, directory or CTest label appears. A quick
self-check, limited to what your change touches: list the suites in the test
files you added or modified (`git diff --name-only HEAD -- tests` plus any new
files, then `grep -hoE 'BOOST_AUTO_TEST_SUITE\(\w+\)'` on them) and confirm each
`<Name>` and its case count appear here. (Older per-structure-type suites such
as `<Type>EntryUnitTests` are covered only at group level above; backfilling
individual rows is welcome but is not required by an unrelated change.) Manual-only checks live in the separate manual plans
(for the GUI: [manual-testing.md](manual-testing.md)) and are linked, not
duplicated, here.

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
| `DocSamplesTests` (`doc_samples_test.cpp`) | `GoldenOutputsMatchFixtureRender`: rendering the synthetic doc-sample fixture through each built-in formatter matches the golden files in `tests/data/output_formats/`; `FixtureCoversEveryFieldValueAlternative`: the fixture exercises every `FieldValue` alternative | Doc samples (and `scripts/check_doc_samples.py`'s embedded excerpts) drifting from what the formatters really emit; a new `FieldValue` alternative that no sample covers |
| Formatter suites (`output_formatter_registry_test.cpp`, `built_in_formatter_registry_test.cpp`, `text_`/`xml_`/`json_`/`yaml_`/`csv_formatter_test.cpp`, `formatter_escaping_test.cpp`) | `OutputFormatterRegistryUnitTests`, `BuiltInFormatterRegistryUnitTests`, `{Text,Xml,Json,Yaml,Csv}FormatterUnitTests`, `FormatterEscapingUnitTests`: registry lookup/duplicate rules, each format's layout, and per-format escaping | A formatter silently changing its output shape, unescaped control or markup characters corrupting a document, a built-in format failing to register |
| `GuiViewModelUnitTests` (`gui_view_model_test.cpp`, 17 cases) | `smbios::gui::build_entry_tree()` / `entry_type_label()` over `SyntheticTableBuilder` tables: node count equals the walk count (registered, unregistered and OEM types), fields equal a direct `get_fields()`, `Unknown (0xNN)` labels for all 46 `SMBiosHandler` values and unnamed types, hostile strings (`%n%s%x`, `##`, ESC, bidi, invalid UTF-8, 64 KiB) kept raw, five malformed-table shapes plus 500 fixed-seed random tables, duplicate handles stay distinct nodes, empty / end-of-table-only tables, a throwing entry becoming a `decode_failed` node, `bad_alloc` propagating, nodes outliving the `SMBios` | A hostile or malformed table making the GUI crash, skip a structure, merge two structures with the same handle, or show firmware text unsanitised; the view model silently disagreeing with the entry classes' own `get_fields()` |
| `GuiFieldTextUnitTests` (`gui_field_text_test.cpp`, 15 cases) | `display_text()` (display-only sanitising: C0/DEL/C1/bidi replacement edges, embedded NUL, 4096-byte cap incl. suffix, never splitting a code point, idempotence, bounded cost on a 16 MiB input), `field_value_text()` (string / number decimal+hex / bool / byte-vector hex dump with its 1024-byte cap) and `entry_node_label()` | A firmware string reaching ImGui with control or direction-override characters, a multi-megabyte string slowing every frame (the P-1 finding - the bounded-cost cases fail against the old implementation), a hex dump or label growing without bound |
| `GuiCopyAsUnitTests` (`gui_copy_as_test.cpp`, 10 cases) | `render_entry_as()` for every registered formatter (byte-exact `IOutputFormatter::render()` output, including hostile strings; unknown / empty / case-variant names and over-1 MiB output give `nullopt`; exactly 1 MiB allowed), `default_copy_format_index()`, `is_terminal_safe_format()`, `create_entry_for_node()` (index/type/handle re-check, duplicate handles resolved by index) | The "Copy as..." clipboard text diverging from the CLI's formatter output, copying the wrong structure after a stale selection, an oversized copy or a terminal-unsafe default format |

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

CTest-level smoke tests are also classified **Integration** (they run the
built `smbios` binary or an example, not a Boost.Test case):

- `smbios_cli_*` (`tests/integration/CMakeLists.txt`):
  `smbios_cli_unknown_format` (`--format bogus` must exit non-zero),
  `smbios_cli_unknown_format_lists_names` (stderr lists the registered
  formats), and `smbios_cli_format_csv` (`--format csv` header line; reads
  real hardware, so it is the only one that can fail without privileges).
- `smbios_example_*` (`examples/CMakeLists.txt`): the `format_table`
  example for `text`/`xml`/`json`/`yaml`, its unknown-format case,
  `read_fields`, and (when Python 3 is found) the `*_parses` structural
  checks. They degrade by design (no readable table gives a notice, an empty
  document and exit 0), so they are must-pass even without privileges.

### C ABI - `tests/c_abi/` (five plain executables, not Boost.Test)

Each proves one slice of the `smbios_c` shim (milestone 0007) against
the real machine's SMBIOS table: `smbios_c_compile_as_c_test` (the
public header parses under a real C compiler, not just C++ with
`extern "C"` ignored), `smbios_c_example` (a genuine end-to-end C
program - open, iterate, render, close), `smbios_c_open_close_test`
(handle lifecycle and the `SMBIOS_ERR_NO_TABLE_FOUND` path),
`smbios_c_entry_iteration_test` and `smbios_c_typed_getters_test` (every
entry's rendered description, and the five curated typed getters, cross-
checked byte-for-byte against the equivalent direct C++
`SMBiosEntryFactory::create()`/`render_to_description()` calls). The
latter two link `libsmbios` directly (for that cross-check) in addition
to `smbios_c`, and need the same whole-archive link treatment
`smbios_c`'s own `CMakeLists.txt` uses - see that file's comment for why.
Guards against the exact gap discovered while first verifying this
milestone by hand: a binary linking `libsmbios` as a plain static
archive without whole-archive silently drops entry-type registration for
any type never directly named in that binary's own code, so `smbios_c`'s
behavior could silently diverge from the CLI's for exactly those types
without a test exercising every real structure, not just the ones the
shim's own source happens to name.

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
  is false") without needing real hardware or root. Story 01.1 (per-OS
  source libraries) lands the injectable seam's library boundary itself
  - two new unit tests proving `smbios_legacy` links standalone and
  that `smbios::make_default_source_chain()` resolves through `smbios`
  core's link graph - ahead of any real fake `ISmbiosSource`; the mock
  placeholder's remaining blocker after that story is a concrete fake
  source, not a missing architectural seam.
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
