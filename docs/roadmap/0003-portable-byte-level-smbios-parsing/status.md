# Milestone 0003 - Portable Byte-Level SMBIOS Parsing - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|---------------------------------------------------|----------------|-------|
| 01.0  | Endian-safe field accessors                        | ✅ Complete | smbios_unit_test pass (ByteReaderUnitTests: 5 cases); CI confirmation pending |
| 02.0  | Field-table redesign of versioned entries         | 🔶 In progress (1/4 tasks) | smbios_unit_test pass (BiosInformationEntryUnitTests: 3 cases) |
| 03.0  | Table and string walk hardening                    | ⬜ Not started | -     |

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
