# 01 - FieldValue and entry interface

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C1**'s types, and add `get_fields()` to
`AbstractSMBiosEntry` as a **TEMPORARY non-pure virtual** returning an
empty vector, so the build and `smbios_unit_test` stay green before any
entry type is migrated. Task 05 makes it pure virtual.

## Files

- **Create** `libsmbios/include/libsmbios/field_value.hpp` (one header for
  both types, Doxygen on each public type and member per
  `docs/dev/cpp_coding_standard.md`):
  ```cpp
  #pragma once
  #include <cstdint>
  #include <string>
  #include <variant>
  #include <vector>

  namespace smbios {

  using FieldValue = std::variant<std::string, uint64_t, bool, std::vector<uint8_t>>;

  struct NamedField {
      std::string name;
      FieldValue value;
  };

  } // namespace smbios
  ```
- **Modify** `libsmbios/include/libsmbios/abstract_smbios_entry.hpp`:
  `#include <libsmbios/field_value.hpp>` and add
  ```cpp
  /// @brief Structured, renderer-neutral view of the same data
  /// render_to_description() prints. TEMPORARY non-pure default (returns {})
  /// until milestone 0009 Story 01.0 Task 05 makes this pure virtual.
  virtual std::vector<NamedField> get_fields() const { return {}; } // TEMPORARY - removed in Task 05
  ```
  Add a protected helper next to the existing `bitset_to_properties()`.
  It walks the bits in the same ascending order, so field order matches
  the text output:
  ```cpp
  /// @brief One NamedField{label, true} per set bit that has a label in properties_map.
  template <typename T>
  void bitset_to_fields(T properties, const std::map<T, std::string>& properties_map,
                        std::vector<NamedField>& out) const;
  ```
  The `TEMPORARY` comment is required verbatim (a grep target). Task 05's
  success criteria check that it has been removed.

## Functions and Classes

- `smbios::FieldValue`, `smbios::NamedField` - new.
- `AbstractSMBiosEntry::get_fields()` - new, temporarily non-pure.
- `AbstractSMBiosEntry::bitset_to_fields()` - new protected helper.

## Constraints

- `std::variant` (C++17, available under the project's C++20 baseline) -
  no new runtime dependency.
- `std::vector<uint8_t>` is a `FieldValue` alternative specifically for
  fields like the Processor ID (8 raw bytes) or a UUID's raw form, where
  stringifying would be lossy or premature. The renderer decides how to
  represent raw bytes (Story 02.0 uses lowercase hex); the field model does
  not decide for it.
- No existing entry class is modified in this task.

## Tests

- `tests/unit/field_value_test.cpp` (new, picked up by
  `tests/unit/CMakeLists.txt`'s `file(GLOB SOURCES *.cpp)`, own
  `BOOST_AUTO_TEST_SUITE(FieldValueUnitTests)`):
  - Construct a `NamedField` for each `FieldValue` alternative and confirm
    that `std::get<T>`/`std::holds_alternative<T>` behave as expected.
  - Using a minimal test-local subclass of `AbstractSMBiosEntry`, confirm
    that `bitset_to_fields()` emits exactly one `true` field per set,
    labelled bit, in ascending bit order, and skips set bits that have no
    label.

## Success criteria

- [ ] `field_value.hpp` compiles standalone.
- [ ] `AbstractSMBiosEntry::get_fields()` exists as a non-pure virtual
      marked `TEMPORARY`, and the whole build (`libsmbios`, `smbios`,
      `smbios_c`, every test target) compiles unchanged.
- [ ] `smbios_unit_test` passes, including the new `field_value_test.cpp`.
