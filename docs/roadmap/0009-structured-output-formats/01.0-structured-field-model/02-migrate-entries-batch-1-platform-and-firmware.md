# 02 - Migrate entries, batch 1: platform and firmware

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Implement `get_fields()` on the 12 platform-identity, firmware and
configuration entry types. Each implementation is built only from that
type's own existing getters. `get_fields()` stays non-pure (Task 01's
TEMPORARY default) during this task, so the build stays green with
batches 2-4 still unmigrated.

## Entry classes in this batch

| SMBIOS type | Class | Header (`libsmbios/include/libsmbios/tables/`) | Unit test (`tests/unit/`) |
|---|---|---|---|
| 0   | `BiosInformationEntry`              | `bios_information_entry.hpp`              | `bios_information_entry_test.cpp` |
| 1   | `SystemInformationEntry`            | `system_information_entry.hpp`            | `system_information_entry_test.cpp` |
| 2   | `BaseboardInformationEntry`         | `baseboard_information_entry.hpp`         | `baseboard_information_entry_test.cpp` |
| 3   | `SystemEnclosureEntry`              | `system_enclosure_entry.hpp`              | `system_enclosure_entry_test.cpp` |
| 11  | `OemStringsEntry`                   | `oem_strings_entry.hpp`                   | `oem_strings_entry_test.cpp` |
| 12  | `SystemConfigurationOptionsEntry`   | `system_configuration_options_entry.hpp`  | `system_configuration_options_entry_test.cpp` |
| 13  | `FirmwareLanguageInformationEntry`  | `firmware_language_information_entry.hpp` | `firmware_language_information_entry_test.cpp` |
| 32  | `SystemBootInformationEntry`        | `system_boot_information_entry.hpp`       | `system_boot_information_entry_test.cpp` |
| 40  | `AdditionalInformationEntry`        | `additional_information_entry.hpp`        | `additional_information_entry_test.cpp` |
| 45  | `FirmwareInventoryInformationEntry` | `firmware_inventory_information_entry.hpp`| `firmware_inventory_information_entry_test.cpp` |
| 46  | `StringPropertyEntry`               | `string_property_entry.hpp`               | `string_property_entry_test.cpp` |
| 126 | `InactiveEntry`                     | `inactive_entry.hpp`                      | `inactive_entry_test.cpp` |

## Files

- **Modify** each class's header (add `std::vector<NamedField> get_fields()
  const override;` with a Doxygen `@brief`) and its `libsmbios/src/<snake>_entry.cpp`
  (the definition). Illustrative shape:
  ```cpp
  std::vector<NamedField> BiosInformationEntry::get_fields() const {
      std::vector<NamedField> fields;
      fields.push_back({"Vendor", get_vendor_string()});
      fields.push_back({"Version", get_version_string()});
      fields.push_back({"Release Date", get_release_date_string()});
      fields.push_back({"ROM Size", static_cast<uint64_t>(get_rom_size())});
      bitset_to_fields(get_properties(), properties_map_, fields);              // one field per set bit
      bitset_to_fields(get_properties_extension1(), properties_extensions1_map_, fields);
      bitset_to_fields(get_properties_extension2(), properties_extensions2_map_, fields);
      fields.push_back({"BIOS Revision", get_bios_version_string()});
      fields.push_back({"Firmware Revision", get_firmware_version_string()});
      return fields;
  }
  ```
  Write one implementation per class, mirroring that class's existing
  `render_to_description()` field by field: the same fields, in the same
  order, with the same version/length guards, and with bitmask properties
  expanded via `bitset_to_fields()`.
- **Modify** each listed unit test file (see Tests).

## Functions and Classes

- `get_fields()` override on the 12 classes above - new.

## Constraints

These rules are shared by all four batches. Tasks 03-05 refer to them
here.

- **No new byte-level parsing.** Every value comes from a getter that
  already exists. This task repackages what is already decoded. It never
  decodes anything new to fill out the field list. If
  `render_to_description()` prints a value that has no getter, add a
  private getter that reuses the existing decode, rather than calling
  `read_uN` inline in `get_fields()`.
- **A field is present exactly when the text prints it.** A field that
  `render_to_description()` omits for the given `header.length` or
  `SMBiosVersion` is omitted from `get_fields()` too. It is not emitted as
  an empty string or zero.
- **Names.** Field names match the `render_to_description()` left-hand
  labels exactly (e.g. `"Vendor"`, not `"vendor"` or `"BIOS Vendor"`),
  without the trailing colon or tab indentation.
- **No header-type field.** The `"Header type: ..."` line is not a field;
  renderers use `get_type()`.
- **Unique names.** Field names are unique within one entry (C1). Repeated
  groups, such as one OEM string per index or one additional-information
  record per entry, use a 1-based index suffix (`"String 1"`,
  `"String 2"`, ...). Where the text output already numbers them, use its
  label.
- **Value types.**
  - Numbers that the text shows as plain decimal become `uint64_t`.
  - Values the text decorates (units, hex formatting such as `0x...`,
    enum-to-string lookups, `"Not Specified"`/`"Unknown"` sentinels) stay
    `std::string`, using the same string the text prints. This keeps
    structured output semantically identical to `text`.
  - Yes/No and supported/not-supported flags become `bool`.
  - Raw identifiers (UUID raw bytes, Processor ID) may use
    `std::vector<uint8_t>` only where the existing getter already returns
    bytes.
- **Untouched.** `render_to_description()` is not modified.
- **`InactiveEntry`.** Its text output carries no data fields beyond the
  header type and a fixed explanatory sentence, so its `get_fields()`
  explicitly returns an empty vector. This is deliberate, and its test
  asserts it.

## Tests

For each class in this batch, extend its existing
`tests/unit/<snake>_entry_test.cpp`, inside that file's existing
`BOOST_AUTO_TEST_SUITE`, with at least:

- **`<Class>GetFieldsMatchesDecodedValuesTestCase`.** Build the structure
  with the file's existing hand-built buffer helpers, or with
  `smbios::test::SyntheticTableBuilder` (`tests/unit/synthetic_table_builder.hpp`)
  plus `SmbiosTableWalkTestAccess::parse_synthetic_table()` where the file
  already uses that path. Call `get_fields()` and assert every expected
  name, `FieldValue` alternative and value, in order.
- **`<Class>GetFieldsExpandsBitmaskTestCase`.** For types with bitmask
  properties, assert one `true` field per set bit and no comma-joined
  string field.
- **`<Class>GetFieldsHonoursLengthTiersTestCase`.** For types with length
  or version tiers, assert that a short structure omits the gated fields,
  mirroring the file's existing tier cases.
- **Name uniqueness.** In every case, assert that `name` values are
  unique.

These tests need no hardware or privileges. They are unit tests per
`docs/dev/cpp_coding_standard.md`.

## Success criteria

- [ ] All 12 classes in this batch override `get_fields()`.
- [ ] Every new test case passes, and the whole `smbios_unit_test` suite
      stays green.
- [ ] The full build (`libsmbios`, `smbios`, `smbios_c`, all test
      targets) is green on the CI matrix.
- [ ] The `text` output of `smbios` is unchanged (`render_to_description()`
      untouched).
