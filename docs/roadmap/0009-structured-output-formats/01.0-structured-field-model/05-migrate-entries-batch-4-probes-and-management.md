# 05 - Migrate entries, batch 4: probes and management; make `get_fields()` pure virtual

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Tasks 02, 03 and 04 (all other batches must be merged first)

## Objective

1. Implement `get_fields()` on the 11 remaining entry types, which cover
   probes, cooling, remote access, BIS, the management devices and IPMI.
2. In the same change, **flip `AbstractSMBiosEntry::get_fields()` to pure
   virtual** (`= 0`) and delete Task 01's TEMPORARY default body. From
   this point the compiler enforces that every current and future entry
   type implements `get_fields()`, which is ratified decision 3a in
   `status.md`.

## Entry classes in this batch

| SMBIOS type | Class | Header (`libsmbios/include/libsmbios/tables/`) | Unit test (`tests/unit/`) |
|---|---|---|---|
| 26 | `VoltageProbeEntry`                      | `voltage_probe_entry.hpp`                      | `voltage_probe_entry_test.cpp` |
| 27 | `CoolingDeviceEntry`                     | `cooling_device_entry.hpp`                     | `cooling_device_entry_test.cpp` |
| 28 | `TemperatureProbeEntry`                  | `temperature_probe_entry.hpp`                  | `temperature_probe_entry_test.cpp` |
| 29 | `ElectricalCurrentProbeEntry`            | `electrical_current_probe_entry.hpp`           | `electrical_current_probe_entry_test.cpp` |
| 30 | `OutOfBandRemoteAccessEntry`             | `out_of_band_remote_access_entry.hpp`          | `out_of_band_remote_access_entry_test.cpp` |
| 31 | `BootIntegrityServicesEntry`             | `boot_integrity_services_entry.hpp`            | `boot_integrity_services_entry_test.cpp` |
| 34 | `ManagementDeviceEntry`                  | `management_device_entry.hpp`                  | `management_device_entry_test.cpp` |
| 35 | `ManagementDeviceComponentEntry`         | `management_device_component_entry.hpp`        | `management_device_component_entry_test.cpp` |
| 36 | `ManagementDeviceThresholdDataEntry`     | `management_device_threshold_data_entry.hpp`   | `management_device_threshold_data_entry_test.cpp` |
| 38 | `IpmiDeviceInformationEntry`             | `ipmi_device_information_entry.hpp`            | `ipmi_device_information_entry_test.cpp` |
| 42 | `ManagementControllerHostInterfaceEntry` | `management_controller_host_interface_entry.hpp` | `management_controller_host_interface_entry_test.cpp` |

## Files

- **Modify** each class's header and `libsmbios/src/<snake>_entry.cpp`, as
  in Tasks 02-04.
- **Modify** `libsmbios/include/libsmbios/abstract_smbios_entry.hpp`.
  Replace the TEMPORARY default with
  ```cpp
  /// @brief Structured, renderer-neutral view of the same data render_to_description() prints.
  /// Every concrete entry must implement it (pure virtual, milestone 0009 R1).
  virtual std::vector<NamedField> get_fields() const = 0;
  ```
  Remove the `// TEMPORARY - removed in Task 05` comment.
- **Modify every test-local subclass of `AbstractSMBiosEntry`**. They stop
  compiling once the method is pure. Each gains a trivial
  `get_fields() const override { return {}; }`:
  - `FakeTestEntry` in `tests/unit/smbios_entry_factory_test.cpp`;
  - `TestEntry` in `tests/unit/dmi_string_parsing_test.cpp`;
  - the helper subclass added by Task 01 in
    `tests/unit/field_value_test.cpp`.

  Re-grep `: public AbstractSMBiosEntry` across `tests/` at implementation
  time in case more were added since.
- `smbios_c/` needs no change. It does not subclass `AbstractSMBiosEntry`;
  re-confirm with the same grep over `smbios_c/` and `tests/c_abi/`.

## Functions and Classes

- `get_fields()` override on the 11 classes above - new.
- `AbstractSMBiosEntry::get_fields()` - now pure virtual.

## Constraints

- Every constraint in
  [Task 02's Constraints section](02-migrate-entries-batch-1-platform-and-firmware.md#constraints)
  applies unchanged.
- Batch-specific: `ManagementDeviceComponentEntry` and
  `ManagementDeviceThresholdDataEntry` (and the
  `ManagementControllerHostInterfaceEntry` protocol records) reference or
  contain repeated data. Use 1-based indexed names.
- Batch-specific: the probe types (26/28/29) print values with units and
  "Unknown" sentinels (e.g. `0x8000`). Per the value-type rule, those
  fields stay the decorated string the text prints.
- The pure-virtual flip lands in the **same commit** as this batch's
  overrides. Never commit a pure virtual with an unmigrated subclass. The
  build must be green at every commit on the branch.

## Tests

- Same per-class pattern as
  [Task 02's Tests section](02-migrate-entries-batch-1-platform-and-firmware.md#tests),
  for the 11 classes above.
- **Coverage case** (new, in `tests/unit/smbios_entry_factory_test.cpp` or
  a new `tests/unit/get_fields_coverage_test.cpp`).
  - For every one of the 45 registered SMBIOS type values, build a
    valid structure long enough to reach every version and length tier
    (*amended 2026-11-10:* a minimal structure trips existing `assert(false)`
    getters) with `SyntheticTableBuilder`, create it through
    `SMBiosEntryFactory::create()`, and assert `get_fields()` is callable
    and yields unique names.
  - `InactiveEntry` and `BootIntegrityServicesEntry` must yield an empty
    vector, because neither one's text output carries data fields
    (*amended 2026-11-10*). Every other type must yield at least one field.
  - Because `smbios_unit_test` does not force-load `libsmbios`, this test
    TU must `#include` all 45 entry headers and reference each class (as
    the existing entry tests do), so that every registration object file
    is linked.

## Success criteria

- [ ] All 11 classes in this batch override `get_fields()`.
- [ ] `AbstractSMBiosEntry::get_fields()` is `= 0`. A grep for `TEMPORARY`
      in `abstract_smbios_entry.hpp` finds nothing.
- [ ] Every test double compiles with its trivial override. The full build
      (`libsmbios`, `smbios`, `smbios_c`, every test target) is green on
      the CI matrix.
- [ ] The coverage case passes for all 45 types.
