# Milestone 0004 - Entry Model and Factory Modernization - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                        | Status         | Tests |
|-------|-------------------------------|----------------|-------|
| 01.0  | Self-registering factory      | ⬜ Not started | -     |
| 02.0  | Expand structure coverage      | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Depends on milestone 0003's accessor pattern (`read_uN` + length guards)
  landing first, so every entry class this milestone adds is written
  against the final pattern rather than the old `reinterpret_cast`
  technique.
- The eight new structure types (Story 02.0) are exactly the remaining
  members of `SMBios::SMBiosHandler`
  ([smbios.h:104-118](/smbios/include/smbios/smbios.h)) that
  `SMBiosEntryFactory::create()` currently returns `nullptr` for:
  `SystemInformation`, `SystemEnclosure`, `ProcessorInformation`,
  `CacheInformation`, `SystemSlots`, `PhysicalMemoryArray`,
  `MemoryArrayMappedAddress`, `SystemBootInformation`.
