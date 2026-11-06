# Milestone 0003 - Portable Byte-Level SMBIOS Parsing - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|---------------------------------------------------|----------------|-------|
| 01.0  | Endian-safe field accessors                        | ✅ Complete | smbios_unit_test pass (ByteReaderUnitTests: 5 cases); CI confirmation pending |
| 02.0  | Field-table redesign of versioned entries         | 🔶 In progress (2/4 tasks) | smbios_unit_test pass (Bios+MemoryDevice entry suites: 11 cases) |
| 03.0  | Table and string walk hardening                    | 🔶 In progress (1/3 tasks) | smbios_unit_test pass (30 cases incl. SmbiosTableWalkUnitTests: 3); smbios_perf_test now passes too |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- This milestone directly implements the redesign discussed before the
  roadmap was written: replace `reinterpret_cast` onto `#pragma pack(1)`
  version-named structs (`BiosInformationV24`/`V31`,
  `MemoryDeviceV21`-`V28`) with little-endian-explicit, length-guarded byte
  accessors. See `plan.md`'s intro for the three concrete problems this
  fixes (non-portability to big-endian hosts, strict-aliasing UB, and
  linear struct proliferation per SMBIOS revision).
- **Resolved:** `PortConnectionEntry` ([port_connection_entry.h](/smbios/include/smbios/port_connection_entry.h))
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
