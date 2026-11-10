# Story 01.0 - Structured Field Model

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Milestones 0004 (self-registering entry factory) and 0010 (all 45 entry types)

## Scope

Define `FieldValue`/`NamedField` (contract **C1**), add `get_fields()` to
`AbstractSMBiosEntry`, and implement it on all 45 registered entry types -
the foundation every renderer in Story 02.0/03.0 builds on.

The migration is split into four batches so **every task leaves the build
and `smbios_unit_test` green**:

1. Task 01 adds `get_fields()` as a **TEMPORARY non-pure virtual** returning
   an empty vector, so nothing breaks before any entry is migrated.
2. Tasks 02-05 each migrate one family of entry types and add their unit
   tests.
3. Task 05, the last batch, makes `get_fields()` **pure virtual** (`= 0`)
   and deletes the temporary default. From then on the compiler enforces
   coverage of every current and future entry type.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [FieldValue and entry interface](01-fieldvalue-and-entry-interface.md)   | ✅ Complete (`b5489e9`) | 02, 03, 04, 05 |
| 02 | [Migrate entries, batch 1: platform and firmware](02-migrate-entries-batch-1-platform-and-firmware.md) | ✅ Complete (`07568f5`) | 05     |
| 03 | [Migrate entries, batch 2: processor and memory](03-migrate-entries-batch-2-processor-and-memory.md) | ⬜ Not started | 05     |
| 04 | [Migrate entries, batch 3: slots, devices and power](04-migrate-entries-batch-3-slots-devices-and-power.md) | ⬜ Not started | 05     |
| 05 | [Migrate entries, batch 4: probes and management; make `get_fields()` pure virtual](05-migrate-entries-batch-4-probes-and-management.md) | ⬜ Not started | 06     |
| 06 | [Tests](06-tests.md)                                                   | ⬜ Not started | -      |

Tasks 02, 03 and 04 are independent of each other once Task 01 has landed,
and may run in any order or in parallel. Task 05 is last because it flips
the method to pure virtual, which only compiles once Tasks 02-04 are done.

## Entry types per batch

All 45 classes derive directly from `AbstractSMBiosEntry` (no intermediate
base). Header: `libsmbios/include/libsmbios/tables/<snake>_entry.hpp`.
Source and `REGISTER_SMBIOS_ENTRY`: `libsmbios/src/<snake>_entry.cpp`.
Existing unit test: `tests/unit/<snake>_entry_test.cpp`.

| Batch (task) | Count | Entry classes (SMBIOS type) |
|--------------|-------|-----------------------------|
| 1 (02) | 12 | `BiosInformationEntry` (0), `SystemInformationEntry` (1), `BaseboardInformationEntry` (2), `SystemEnclosureEntry` (3), `OemStringsEntry` (11), `SystemConfigurationOptionsEntry` (12), `FirmwareLanguageInformationEntry` (13), `SystemBootInformationEntry` (32), `AdditionalInformationEntry` (40), `FirmwareInventoryInformationEntry` (45), `StringPropertyEntry` (46), `InactiveEntry` (126) |
| 2 (03) | 10 | `ProcessorInformationEntry` (4), `CacheInformationEntry` (7), `PhysicalMemoryArrayEntry` (16), `MemoryDeviceEntry` (17), `MemoryErrorInformation32BitEntry` (18), `MemoryArrayMappedAddressEntry` (19), `MemoryDeviceMappedAddressEntry` (20), `MemoryErrorInformation64BitEntry` (33), `MemoryChannelEntry` (37), `ProcessorAdditionalInformationEntry` (44) |
| 3 (04) | 12 | `PortConnectionEntry` (8), `SystemSlotsEntry` (9), `GroupAssociationsEntry` (14), `SystemEventLogEntry` (15), `PointingDeviceEntry` (21), `PortableBatteryEntry` (22), `SystemResetEntry` (23), `HardwareSecurityEntry` (24), `SystemPowerControlsEntry` (25), `SystemPowerSupplyEntry` (39), `OnboardDevicesExtendedInformationEntry` (41), `TpmDeviceEntry` (43) |
| 4 (05) | 11 | `VoltageProbeEntry` (26), `CoolingDeviceEntry` (27), `TemperatureProbeEntry` (28), `ElectricalCurrentProbeEntry` (29), `OutOfBandRemoteAccessEntry` (30), `BootIntegrityServicesEntry` (31), `ManagementDeviceEntry` (34), `ManagementDeviceComponentEntry` (35), `ManagementDeviceThresholdDataEntry` (36), `IpmiDeviceInformationEntry` (38), `ManagementControllerHostInterfaceEntry` (42) |

Total: 12 + 10 + 12 + 11 = 45, one per `REGISTER_SMBIOS_ENTRY` in
`libsmbios/src/`. Task 05 also updates every test-local subclass:
`FakeTestEntry` (`tests/unit/smbios_entry_factory_test.cpp`), `TestEntry`
(`tests/unit/dmi_string_parsing_test.cpp`), and the helper subclass Task 01
adds in `tests/unit/field_value_test.cpp`.

## Key constraints

- **End state:** `get_fields()` is pure virtual, with no default/fallback
  implementation that would let a future entry type ship without
  structured-output support. The non-pure default from Task 01 is
  **TEMPORARY scaffolding** that exists only so Tasks 02-04 can each land
  green. Task 05 deletes it, and no task other than Task 05 may mark the
  story ✅.
- `get_fields()` must not re-derive any value independently of the
  existing typed getters (R1). It calls `get_vendor_string()`,
  `get_properties()` and so on, exactly as `render_to_description()`
  already does, and packages the results as `NamedField`s instead of
  writing them into prose.
- A bitmask property becomes one `NamedField` per set bit, not one
  comma-joined string field (per `plan.md`'s C1 note), produced through
  Task 01's shared `bitset_to_fields()` helper. This is the one place the
  story changes *shape*, not just *packaging*, compared with the existing
  `render_to_description()` text.
- `render_to_description()` is not modified anywhere in this story. The
  `text` format (Story 02.0) depends on it staying byte-identical.

## Files or directories created or modified by this story

- `libsmbios/include/libsmbios/field_value.hpp` (new; `FieldValue` and
  `NamedField`). It is picked up by the existing
  `include/libsmbios/*.hpp` glob in `libsmbios/src/CMakeLists.txt`, so no
  CMake edit is needed.
- `libsmbios/include/libsmbios/abstract_smbios_entry.hpp`: `get_fields()`
  (temporary non-pure in Task 01, pure in Task 05) and the protected
  `bitset_to_fields()` helper.
- All 45 entry headers under `libsmbios/include/libsmbios/tables/` and
  sources under `libsmbios/src/`: each gains a `get_fields()` override.
- `tests/unit/field_value_test.cpp` (new) and each of the 45 existing
  `tests/unit/<snake>_entry_test.cpp` files (extended).
- `tests/unit/smbios_entry_factory_test.cpp` and
  `tests/unit/dmi_string_parsing_test.cpp`: test doubles gain
  `get_fields()` (Task 05).
- Not touched: `smbios_c/` (consumer only, no subclass) and `smbios/` (the
  CLI is wired up in Story 03.0).
