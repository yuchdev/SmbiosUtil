# Milestone 0003 - Portable Byte-Level SMBIOS Parsing - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|---------------------------------------------------|----------------|-------|
| 01.0  | Endian-safe field accessors                        | ⬜ Not started | -     |
| 02.0  | Field-table redesign of versioned entries         | ⬜ Not started | -     |
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
