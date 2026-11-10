# 04 - Migrate entries, batch 3: slots, devices and power

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (independent of Tasks 02 and 03)

## Objective

Implement `get_fields()` on the 12 types in this batch: connector, slot,
on-board and peripheral devices, event log, security, and power entry
types. Each implementation is built only from that type's own existing
getters. `get_fields()` stays non-pure (Task 01's TEMPORARY default)
during this task.

## Entry classes in this batch

| SMBIOS type | Class | Header (`libsmbios/include/libsmbios/tables/`) | Unit test (`tests/unit/`) |
|---|---|---|---|
| 8  | `PortConnectionEntry`                    | `port_connection_entry.hpp`                    | `port_connection_entry_test.cpp` |
| 9  | `SystemSlotsEntry`                       | `system_slots_entry.hpp`                       | `system_slots_entry_test.cpp` |
| 14 | `GroupAssociationsEntry`                 | `group_associations_entry.hpp`                 | `group_associations_entry_test.cpp` |
| 15 | `SystemEventLogEntry`                    | `system_event_log_entry.hpp`                   | `system_event_log_entry_test.cpp` |
| 21 | `PointingDeviceEntry`                    | `pointing_device_entry.hpp`                    | `pointing_device_entry_test.cpp` |
| 22 | `PortableBatteryEntry`                   | `portable_battery_entry.hpp`                   | `portable_battery_entry_test.cpp` |
| 23 | `SystemResetEntry`                       | `system_reset_entry.hpp`                       | `system_reset_entry_test.cpp` |
| 24 | `HardwareSecurityEntry`                  | `hardware_security_entry.hpp`                  | `hardware_security_entry_test.cpp` |
| 25 | `SystemPowerControlsEntry`               | `system_power_controls_entry.hpp`              | `system_power_controls_entry_test.cpp` |
| 39 | `SystemPowerSupplyEntry`                 | `system_power_supply_entry.hpp`                | `system_power_supply_entry_test.cpp` |
| 41 | `OnboardDevicesExtendedInformationEntry` | `onboard_devices_extended_information_entry.hpp` | `onboard_devices_extended_information_entry_test.cpp` |
| 43 | `TpmDeviceEntry`                         | `tpm_device_entry.hpp`                         | `tpm_device_entry_test.cpp` |

## Files

- **Modify** each class's header (add `std::vector<NamedField> get_fields()
  const override;` with a Doxygen `@brief`) and its `libsmbios/src/<snake>_entry.cpp`.
  Write one implementation per class, mirroring its
  `render_to_description()` field by field.
- **Modify** each listed unit test file.

## Functions and Classes

- `get_fields()` override on the 12 classes above - new.

## Constraints

- Every constraint in
  [Task 02's Constraints section](02-migrate-entries-batch-1-platform-and-firmware.md#constraints)
  applies unchanged.
- Batch-specific: some entries contain variable-length repeated records:
  - `GroupAssociationsEntry` items;
  - `SystemEventLogEntry` supported-event-log-type descriptors;
  - `SystemSlotsEntry` peer groups.

  These need 1-based indexed names. Group a record's sub-values under one
  index (e.g. `"Item 1 Type"`, `"Item 1 Handle"`), so that names stay
  unique and readable.
- Batch-specific: `SystemSlotsEntry` characteristics bytes are bitmasks.
  Expand them via `bitset_to_fields()`.

## Tests

Same pattern as
[Task 02's Tests section](02-migrate-entries-batch-1-platform-and-firmware.md#tests).
Each listed `tests/unit/<snake>_entry_test.cpp` gains a
`<Class>GetFieldsMatchesDecodedValuesTestCase`. Where applicable, it also
gains bitmask-expansion and length/version-tier cases, plus a name
uniqueness assertion. For the repeated-record types, include a case with
at least two records, to prove the index suffixes.

## Success criteria

- [ ] All 12 classes in this batch override `get_fields()`.
- [ ] Every new test case passes, and `smbios_unit_test` stays green.
- [ ] The full build is green on the CI matrix. The `text` output is
      unchanged.
