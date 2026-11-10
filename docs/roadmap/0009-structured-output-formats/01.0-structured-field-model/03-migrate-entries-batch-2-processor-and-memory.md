# 03 - Migrate entries, batch 2: processor and memory

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (independent of Tasks 02 and 04)

## Objective

Implement `get_fields()` on the 10 processor, cache and memory entry
types. Each implementation is built only from that type's own existing
getters. `get_fields()` stays non-pure (Task 01's TEMPORARY default)
during this task.

## Entry classes in this batch

| SMBIOS type | Class | Header (`libsmbios/include/libsmbios/tables/`) | Unit test (`tests/unit/`) |
|---|---|---|---|
| 4  | `ProcessorInformationEntry`           | `processor_information_entry.hpp`            | `processor_information_entry_test.cpp` |
| 7  | `CacheInformationEntry`               | `cache_information_entry.hpp`                | `cache_information_entry_test.cpp` |
| 16 | `PhysicalMemoryArrayEntry`            | `physical_memory_array_entry.hpp`            | `physical_memory_array_entry_test.cpp` |
| 17 | `MemoryDeviceEntry`                   | `memory_device_entry.hpp`                    | `memory_device_entry_test.cpp` |
| 18 | `MemoryErrorInformation32BitEntry`    | `memory_error_information_32bit_entry.hpp`   | `memory_error_information_32bit_entry_test.cpp` |
| 19 | `MemoryArrayMappedAddressEntry`       | `memory_array_mapped_address_entry.hpp`      | `memory_array_mapped_address_entry_test.cpp` |
| 20 | `MemoryDeviceMappedAddressEntry`      | `memory_device_mapped_address_entry.hpp`     | `memory_device_mapped_address_entry_test.cpp` |
| 33 | `MemoryErrorInformation64BitEntry`    | `memory_error_information_64bit_entry.hpp`   | `memory_error_information_64bit_entry_test.cpp` |
| 37 | `MemoryChannelEntry`                  | `memory_channel_entry.hpp`                   | `memory_channel_entry_test.cpp` |
| 44 | `ProcessorAdditionalInformationEntry` | `processor_additional_information_entry.hpp` | `processor_additional_information_entry_test.cpp` |

## Files

- **Modify** each class's header (add `std::vector<NamedField> get_fields()
  const override;` with a Doxygen `@brief`) and its `libsmbios/src/<snake>_entry.cpp`.
  Write one implementation per class, mirroring its
  `render_to_description()` field by field.
- **Modify** each listed unit test file.

## Functions and Classes

- `get_fields()` override on the 10 classes above - new.

## Constraints

- Every constraint in
  [Task 02's Constraints section](02-migrate-entries-batch-1-platform-and-firmware.md#constraints)
  applies unchanged: no new parsing, a field is present exactly when the
  text prints it, exact label names, no header-type field, unique names,
  the value-type rules, and `render_to_description()` untouched.
- Batch-specific: `ProcessorInformationEntry`'s Processor ID and any
  size/speed fields follow the value-type rule.
  - A size or speed that the text decorates with a unit stays the
    decorated string.
  - The Processor ID becomes `std::vector<uint8_t>` only if an existing
    getter already returns its raw bytes. Otherwise it is the string the
    text prints.
- Batch-specific: `MemoryChannelEntry` and
  `ProcessorAdditionalInformationEntry` contain repeated per-item records.
  Name them with 1-based indices, per the unique-names rule.

## Tests

Same pattern as
[Task 02's Tests section](02-migrate-entries-batch-1-platform-and-firmware.md#tests).
Each listed `tests/unit/<snake>_entry_test.cpp` gains a
`<Class>GetFieldsMatchesDecodedValuesTestCase`. Where applicable, it also
gains bitmask-expansion and length/version-tier cases, plus a name
uniqueness assertion. All use synthetic buffers
(`tests/unit/synthetic_table_builder.hpp` or the file's existing helpers).

## Success criteria

- [ ] All 10 classes in this batch override `get_fields()`.
- [ ] Every new test case passes, and `smbios_unit_test` stays green.
- [ ] The full build is green on the CI matrix. The `text` output is
      unchanged.
