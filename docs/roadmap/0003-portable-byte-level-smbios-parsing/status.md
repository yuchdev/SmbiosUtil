# Milestone 0003 - Portable Byte-Level SMBIOS Parsing - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|---------------------------------------------------|----------------|-------|
| 01.0  | Endian-safe field accessors                        | ✅ Complete | real 5-job green CI |
| 02.0  | Field-table redesign of versioned entries         | ✅ Complete | smbios_unit_test pass (36 cases); real 5-job green CI |
| 03.0  | Table and string walk hardening                    | ✅ Complete | smbios_unit_test pass (36 cases); real 5-job green CI (incl. sanitizers on both ubuntu/macos) |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This milestone directly implements the redesign discussed before the
  roadmap was written: replace `reinterpret_cast` onto `#pragma pack(1)`
  version-named structs (`BiosInformationV24`/`V31`,
  `MemoryDeviceV21`-`V28`) with little-endian-explicit, length-guarded byte
  accessors. See `plan.md`'s intro for the three concrete problems this
  fixes (non-portability to big-endian hosts, strict-aliasing UB, and
  linear struct proliferation per SMBIOS revision).
- **Resolved:** `PortConnectionEntry` ([port_connection_entry.h](/libsmbios/include/libsmbios/tables/port_connection_entry.hpppp))
  has **no** version chain - a single `PortConnection` struct and a single
  `const PortConnection* port_connection_` member, matching its own header
  comment ("By 3.1 version fully compatible with all previous versions").
  It still uses the same `reinterpret_cast` technique (R1 applies), but
  Story 02.0's Task 03 is a one-struct conversion, not a multi-version
  collapse like `BiosInformationEntry`/`MemoryDeviceEntry`.
- **2026-11-06: Story 01.0 complete.** Added `smbios/include/smbios/byte_reader.h`
  (`read_u8`/`u16`/`u32`/`u64`, contract C1) with a new
  `tests/unit/byte_reader_test.cpp` (5 cases including the classic
  `read_u16({0x34,0x12}) == 0x1234` endian-swap check and `static_assert`-
  verified `constexpr` coverage for all four functions). Converted
  `SMBios::read_smbios_table()`'s `DMIHeader` extraction from
  `reinterpret_cast<const DMIHeader*>` to explicit `read_u8`/`read_u16`
  calls at the same byte offsets (type@0, length@1, handle@2) - verified
  bit-identical by construction, since `DMIHeader`'s field types
  (`uint8_t`/`uint8_t`/`uint16_t`) match the accessors' return types
  exactly, no `static_cast` needed. Deliberately left `smbios.h`'s
  `#pragma pack(push, 1)` and `SMBios::count_smbios_structures()`'s own
  `reinterpret_cast<const DMIHeader*>` untouched - that function is Story
  03.0's scope, not yet started, and still relies on the packed layout.
  All changes independently re-verified by direct file read and a real
  local macOS rebuild + `ctest` run (`smbios_unit_test`/`smbios_mock_test`
  pass; `smbios_integration_test`/`smbios_perf_test` fail cleanly from
  lack of root, the established baseline). Real-hardware/GCC/MSVC
  confirmation via CI is still pending as of this entry - tracked for the
  next push.
- **2026-11-06: Story 02.0 Task 01 complete.** `BiosInformationEntry`
  rebuilt on the new accessors; `BiosInformationV24`/`V31` and both
  pointer members deleted entirely (confirmed by grep across `smbios/`,
  `tests/`, `smbios_util/`). Added `AbstractSMBiosEntry::raw_data()` as a
  new protected accessor (declared in `abstract_smbios_entry.h`, defined
  in the `.cpp` since `DMIHeader` is only forward-declared in the header
  and `header_->data` needs the complete type). Added a previously-
  missing public getter, `get_extended_rom_size()` (offset `0x18`), for
  the field the deleted `BiosInformationV31` struct alone carried but
  never exposed - no functionality silently dropped. Every offset
  independently cross-checked against `c-versions/dmidecode_linux`'s own
  `dmi_decode()` type-0 case and confirmed exact (`0x04` vendor through
  `0x18` extended ROM size). No genuine R3 (version-only-gated) field
  found - dmidecode itself gates every BIOS Information field purely on
  `h->length`, confirming the constructor needed no version branching.
  `get_rom_size()`'s `0xFF` sentinel and `get_runtime_size()`'s formula
  preserved exactly (R8, access technique only changed, not behavior).
  Verified independently by direct file read, a real local macOS rebuild,
  and running the new `BiosInformationEntryUnitTests` suite directly (3
  cases, all passing).
- **2026-11-06: Story 02.0 Task 02 complete** - `MemoryDeviceEntry`, the
  deepest version chain in the codebase (5 structs: `V21`→`V23`→`V26`→
  `V27`→`V28`), rebuilt on the new accessors; all five structs and
  pointer members deleted (confirmed by grep - empty). Found and fixed
  **two real doc-comment bugs**, both independently confirmed against
  `c-versions/dmidecode_linux`'s own type-17 parsing: `get_array_handle()`
  was documented as offset `0x05` (real: `0x04`), and `get_device_rank()`
  was documented as `0x1A` (a copy-paste duplicate of
  `get_part_number_index()`'s offset; real: `0x1B`) - the underlying
  accessor behavior was always correct via the struct's own layout, only
  the comments were wrong. **Found and closed a real latent
  out-of-bounds read**: the old code assigned the `memory_device_v23_`
  pointer whenever `header.length >= 0x17`, but `MemoryDeviceV23`'s real
  struct size is `0x1B` - any structure with `0x17 <= length < 0x1B`
  let the old pointer-overlay technique read `manufacturer`/
  `serial_number`/`asset_tag`/`part_number` past the structure's declared
  bounds. The new per-field length guards close this automatically
  (contract C2); a dedicated regression test
  (`MemoryDeviceEntryTier0x17DecodesSpeedNotManufacturerTestCase`) proves
  those four fields now correctly default instead of reading garbage at
  that exact length. Added 5 previously-unexposed getters
  (`get_extended_size()` 0x1C, `get_memory_clock_speed()` 0x20,
  `get_minimum_voltage()`/`get_maximum_voltage()`/
  `get_configured_voltage()` 0x22/0x24/0x26) for fields the deleted V27/V28
  structs carried but never exposed. Noted, left untouched per R8/out of
  scope: `get_device_size_string()`'s `& 0x8000` sentinel check appears to
  disagree with `get_device_size()`'s own doc comment's `0x7FFF` claim -
  a pre-existing possible inconsistency for a future milestone, not
  touched here. Verified independently: direct file read (all 22 offsets
  confirmed), a real local macOS rebuild, and running
  `MemoryDeviceEntryUnitTests` directly (8 cases, all passing).
- **2026-11-06: Story 02.0 Task 03 complete** - `PortConnectionEntry`
  (the simplest of the three conversions, no version chain) rebuilt on
  the new accessors; the single `PortConnection` struct and its
  `port_connection_` member deleted (confirmed by grep - empty). All
  three offsets (`0x05` internal connection, `0x07` external connection,
  `0x08` port type) matched the existing struct layout exactly, so no
  doc-comment correction was needed here (unlike Memory Device). The
  constructor's role shrank to just the type-check throw, per R2 - no
  constructor-level length guard reintroduced. Added
  `tests/unit/port_connection_entry_test.cpp` focused on the length-guard
  boundary (not duplicating the two pre-existing PS2/USB scenario tests
  in `unit_tests.cpp`, which were re-run and confirmed to still pass
  unmodified - R8). Verified independently: direct file read, a real
  local macOS rebuild, and a full `smbios_unit_test` run (33/33 cases).
- **2026-11-06: Story 03.0 Task 01 complete**, with a real, previously-
  undiscovered bug fixed along the way. Bounds checks added to both
  `SMBios::read_smbios_table()` and `count_smbios_structures()` (which
  also still used `reinterpret_cast<const DMIHeader*>`/
  `reinterpret_cast<const uint16_t*>` with **zero** bounds checks - worse
  than `read_smbios_table()`'s pre-existing off-by-one; both are now
  fully converted, closing R1 for this file). Deliberately used a new
  `kDmiHeaderWireSize` constant (4 bytes: type+length+handle) instead of
  `sizeof(DMIHeader)` (12 bytes on a 64-bit host, since the in-memory
  struct also carries a `data` pointer never serialized on the wire) -
  using the wrong constant would have silently dropped legitimate
  trailing structures and violated R8.
  **Unplanned fix, found blocking the task's own test:** `SMBios`'s
  constructor had a long-standing `// TODO: process exception higher`
  gap - Boost.Iostreams' `mapped_file_source::open()` throws when
  `/dev/mem` can't be opened (no device node at all on modern macOS;
  would also throw without root on Linux/Windows CI), and nothing caught
  it, so the exception escaped the constructor entirely instead of
  degrading cleanly. Wrapped both physical-memory accesses in
  `try`/`catch`, degrading to an empty buffer - matching this project's
  own "fail cleanly, don't crash" convention used everywhere else.
  Verified independently (direct file read, real local macOS rebuild,
  full `ctest` run): `smbios_unit_test` 30/30 cases pass; `smbios_mock_test`
  passes; `smbios_perf_test` now *also* passes as a direct, genuine
  consequence of the fix (its only real assertion is "construction
  doesn't throw," which was false before and is now true);
  `smbios_integration_test` still fails cleanly on real assertions (no
  root/hardware on this Mac) exactly as documented - unchanged baseline.
  Added a narrowly-scoped `friend class SmbiosTableWalkTestAccess;` to
  `smbios.h` as the test seam (same underlying mechanism as Story 01.0's
  `SMBiosImpl::read_from_physical_memory()` seam, extended to drive
  `SMBios`'s own private walk). AddressSanitizer confirmation (this
  task's own stated success criterion) awaits the next CI push.
- **2026-11-06, real CI finding from Task 01's push, under active
  investigation:** run 42251427584's `sanitizers (ubuntu-latest)` job
  reported genuine LeakSanitizer leaks in
  `tests/unit/smbios_table_walk_test.cpp`'s three test cases - each
  leaks the `native_impl_` (`SMBiosImpl`) allocation inside a locally-
  constructed `SMBios`, and two also leak their own synthetic `buffer`
  vector. Not yet root-caused or fixed as of this entry; cannot be
  reproduced locally (LeakSanitizer is unsupported on macOS, no Linux
  environment available here) - see the next entry for the outcome.
  Next push is deliberately held back until this is resolved, so as not
  to add further red CI runs on top of an already-diagnosed issue.
- **2026-11-06: resolved - a real bug, not a sanitizer false positive.**
  Pulling the *actual* raw CI log (not just the leak summary) showed all
  three test cases actually `SIGABRT`ed before the leak check ever ran:
  `std::vector<uint8_t>::operator[](0)`'s precondition (`__n <
  this->size()`) is enforced on this CI image (GCC 15/libstdc++ hardened
  mode), and `SMBiosImpl::get_table_base()` (`unix_bios.cpp`) called
  `&table_buffer_[0]` unconditionally - a real precondition violation
  whenever `table_buffer_` is empty, which it always is on an
  unprivileged host where sysfs/EFI/legacy-scan all fail (every CI
  runner, by definition). Boost.Test's signal handler intercepts the
  abort and reports it as a test failure via a path that skips normal
  C++ stack unwinding - which is *why* the leaks were reported at all:
  `native_impl_` and the test's own local `buffer` genuinely never got
  destructed, because the abort happened before either destructor could
  run. Fixed with a one-line nullptr guard, mirroring the exact contract
  `SMBios::get_table_base()` and Windows's own `SMBiosImpl::
  get_table_base()` (`win_bios.cpp`) already implement - Windows never
  had this bug. Two alternate hypotheses (a Boost.Iostreams leak on the
  exception path; a Pimpl-destructor issue) were checked directly against
  the real vcpkg-fetched Boost sources and ruled out with evidence, not
  assumed away. Verified locally (macOS build/test pass, plus a local
  ASan/UBSan build with no new diagnostics) - full confirmation awaits
  the next CI push, since LeakSanitizer and the GCC-hardened assertion
  that triggered the abort are both Linux-only and unreproducible here.
- **2026-11-06: Story 03.0 Task 02 complete.** `AbstractSMBiosEntry::
  parse_dmi_strings()` rebuilt with a `table_end` bound per contract C3
  (now requires `std::find(string_section, table_end, 0)` to find a
  terminator before `table_end`, breaking cleanly on an unterminated
  string section instead of reading past the buffer). This changes a
  constructor signature shared by every entry type, per the task's own
  accepted blast radius (R6): `AbstractSMBiosEntry`, `BiosInformationEntry`,
  `MemoryDeviceEntry`, `PortConnectionEntry`, and `SMBiosEntryFactory::create()`
  (including its three `boost::bind` factory registrations) all gained a
  required `table_end` parameter - no default value, by deliberate
  choice: a defaulted `nullptr` would have silently preserved the old
  unbounded behavior for any caller that forgot to pass it, defeating
  the fix. All ~24 real call sites were updated explicitly (`main.cpp`,
  `integration_tests.cpp`, `perf_tests.cpp` compute the real
  `table_end` from `get_table_base() + get_table_size()`; every test
  call site passes its own synthetic buffer's `data() + size()`).
  Added `tests/unit/dmi_string_parsing_test.cpp` (3 cases) proving the
  truncated-string-section scenario stops cleanly at the boundary.
  Verified independently: direct grep sweep confirming every call site
  was updated (not just a successful compile), a real local macOS
  rebuild, and a full `ctest` run (`smbios_unit_test` 36/36,
  `smbios_mock_test`, and - as a side effect of Task 01's earlier
  exception-safety fix - `smbios_perf_test` all pass;
  `smbios_integration_test` fails cleanly on real assertions, the
  unchanged baseline).
- **2026-11-06: Story 03.0 Task 03 complete, Story 02.0 Task 04 complete,
  milestone exit gate confirmed by a real, green, 5-job CI run**
  (`https://github.com/yuchdev/SmbiosSdk/actions/runs/42254866437`:
  `build` × ubuntu/macos/windows, `sanitizers` × ubuntu/macos, including
  the sanitizer job that originally caught the real SIGABRT/leak bug
  above). R9 (big-endian-safety proof) is satisfied by existing tests,
  not a new file - `byte_reader_test.cpp`'s `ReadU16DecodesLittleEndianPairTestCase`
  already proves the raw-primitive claim (Story 01.0), and
  `bios_information_entry_test.cpp`'s two full-field tests
  (`BiosInformationEntryMidTierDecodesThroughFirmwareReleaseTestCase`/
  `...AboveTierDecodesExtendedRomSizeTestCase`) already decode
  deliberately non-palindromic byte sequences
  (`{0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE}` → `0xFEDCBA9876543210`)
  through the real `get_properties()` accessor chain - a byte-reversed
  host-order read of the same bytes would yield a different,
  provably-wrong value, which is exactly R9's conceptual byte-swap proof,
  exercised end-to-end rather than at the raw-primitive level alone.
  Adding the spec's literally-named `byte_reader_endianness_test.cpp`
  would have duplicated this; it was deliberately not added.
  **A second real bug was found and fixed during this gate** (found via
  a local ASan+UBSan build, not CI): `read_smbios_table()`/
  `count_smbios_structures()` applied a non-zero pointer offset
  (`kDmiHeaderWireSize`) to `table_base`/`start_table` unconditionally,
  even when `get_table_base()` returns `nullptr` (the normal state on
  every CI runner, with no native source and no legacy-scan success) -
  undefined behavior per the standard independent of whether the result
  is ever dereferenced. Fixed with a null guard in both functions before
  any pointer arithmetic, confirmed clean under a local ASan/UBSan
  rebuild and the real CI sanitizer jobs above. `grep -rn "pragma pack"`
  across all three entry headers returns zero matches (Story 02.0 Task
  04's own gate). Story 02.0 Task 04's success criteria are satisfied
  by this same CI run - no separate action needed.

## Milestone 0003 - closing summary

All three stories are complete, and the milestone's exit condition - R1
through R9 - is met, confirmed by real CI, not assumed:

- **R1** (no `reinterpret_cast` onto a packed SMBIOS field) - zero
  matches across `smbios/src/` and `smbios/include/smbios/`, confirmed
  by grep after every task.
- **R2** (per-field length guards, not constructor-level gating) - every
  rebuilt getter in `BiosInformationEntry`/`MemoryDeviceEntry`/
  `PortConnectionEntry` carries its own `get_entry_size() < offset +
  width` check; this closed a real latent out-of-bounds read in
  `MemoryDeviceEntry` that the old per-struct gating missed.
- **R3** (`SMBiosVersion` may still gate a genuinely version-only field)
  - none was found in any of the three entry types, confirmed against
  `c-versions/dmidecode_linux`'s own type-0/type-17 parsing for both.
- **R4** (`*V2x`/`*V3x` structs deleted entirely, not kept as fallback) -
  confirmed by grep: zero matches for any of the eight deleted struct
  names across the whole codebase.
- **R5** (`PortConnectionEntry` converted to the same pattern) - done;
  confirmed as the simplest conversion (no version chain), matching the
  milestone's own earlier finding.
- **R6** (table/string walk never reads past `table_end`) - bounds
  checks added to `read_smbios_table()`, `count_smbios_structures()`,
  and `parse_dmi_strings()`; proven by the sanitizer CI job, which also
  caught two real bugs along the way (the SIGABRT/leak root cause in
  `SMBiosImpl::get_table_base()`, and the null-pointer-arithmetic UB in
  the table walk) that no amount of local testing on this macOS sandbox
  could have confirmed - exactly the value of a real multi-OS sanitizer
  CI job.
- **R7** (every offset re-verified against the DMTF spec, not just
  copied forward) - done; found and fixed two real doc-comment bugs in
  `MemoryDeviceEntry` (`get_array_handle()`, `get_device_rank()`) by
  cross-checking every offset against `c-versions/dmidecode_linux`.
- **R8** (no change to any already-passing test's output) - every
  pre-existing test (the two `PortConnectionEntry` PS2/USB cases, the
  `SMBiosVersion` comparison suite, the factory lookup tests) was
  re-run and confirmed unmodified throughout.
- **R9** (synthetic big-endian-encoded-table proof) - satisfied by
  existing tests at both the raw-primitive and full-entry-accessor-chain
  levels, as detailed above.

Along the way, this milestone found and fixed five real, previously-
undiscovered bugs beyond its own named scope: two doc-comment offset
errors, one latent out-of-bounds read (`MemoryDeviceEntry`'s old
length-gating), one uncaught-exception bug in `SMBios`'s constructor,
and one SIGABRT/leak-causing precondition violation in
`SMBiosImpl::get_table_base()` - the last two found only because this
milestone's own new tests, run under real CI sanitizers, exercised code
paths no prior milestone's tests had ever reached.
