# 02 - Migrate entries to structured fields

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Implement `get_fields()` on all eleven entry types from milestone 0004
(`BiosInformationEntry`, `PortConnectionEntry`, `MemoryDeviceEntry`,
`SystemInformationEntry`, `SystemEnclosureEntry`,
`ProcessorInformationEntry`, `CacheInformationEntry`, `SystemSlotsEntry`,
`PhysicalMemoryArrayEntry`, `MemoryArrayMappedAddressEntry`,
`SystemBootInformationEntry`), each built from that type's own existing
getters.

## Files

- **Modify** every entry type's `.cpp` file, e.g.:
  ```cpp
  std::vector<NamedField> BiosInformationEntry::get_fields() const {
      std::vector<NamedField> fields;
      fields.push_back({"Vendor", get_vendor_string()});
      fields.push_back({"Version", get_version_string()});
      fields.push_back({"Release Date", get_release_date_string()});
      fields.push_back({"ROM Size", static_cast<uint64_t>(get_rom_size())});
      // one NamedField per set bit, not a joined string:
      for (const auto& [bit, label] : properties_map_) {
          if (get_properties() & bit) fields.push_back({label, true});
      }
      // ... properties_extensions1_map_/properties_extensions2_map_ likewise ...
      fields.push_back({"BIOS Revision", get_bios_version_string()});
      fields.push_back({"Firmware Revision", get_firmware_version_string()});
      return fields;
  }
  ```
  one such implementation per entry type, mirroring that type's existing
  `render_to_description()` field-by-field (same fields, same order, bitmask
  properties expanded per `plan.md`'s C1 note).

## Functions and Classes

- `get_fields()` on all eleven `AbstractSMBiosEntry` subclasses - new.

## Constraints

- **No new byte-level parsing.** Every value in every `get_fields()`
  implementation comes from a getter that already exists - this task is
  purely "repackage what's already decoded," never "decode something new
  to fill out the field list."
- Field names match the existing `render_to_description()`'s left-hand
  labels exactly (e.g. `"Vendor"`, not `"vendor"` or `"BIOS Vendor"`) -
  this keeps `text` format (Story 02.0, which reuses
  `render_to_description()` verbatim) and the structured formats
  describing the same underlying concept under the same name, so a reader
  comparing both outputs isn't confused by a naming mismatch.

## Tests

- `tests/unit/<entry_type>_fields_test.cpp` (new, one per entry type, or
  extended into each entry type's existing test file from milestone
  0003/0004/0006): construct via `SyntheticTableBuilder` (milestone
  0006), call `get_fields()`, confirm every expected field name/value pair
  is present and every bitmask property expands to individual `true`
  entries rather than a joined string.

## Success criteria

- [ ] All eleven entry types implement `get_fields()`; the codebase
      compiles again (resolving Task 01's expected intermediate
      break).
- [ ] Every entry type's field count/names/values are verified by test
      against a synthetic fixture.
