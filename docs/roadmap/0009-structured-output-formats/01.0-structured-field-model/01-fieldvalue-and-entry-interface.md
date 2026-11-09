# 01 - FieldValue and entry interface

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C1** exactly, and add the pure virtual
`get_fields()` declaration to `AbstractSMBiosEntry`.

## Files

- **Create** `smbios/include/smbios/field_value.h`:
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
- **Modify** `smbios/include/smbios/abstract_smbios_entry.hpppp`: add
  ```cpp
  virtual std::vector<NamedField> get_fields() const = 0;
  ```
  Making this pure virtual is itself a breaking change to
  `AbstractSMBiosEntry`'s interface - every existing subclass must gain an
  implementation in Task 02 before the codebase compiles again; this task
  and Task 02 land together, not independently buildable.

## Functions and Classes

- `smbios::FieldValue`, `smbios::NamedField` - new.
- `AbstractSMBiosEntry::get_fields()` - new pure virtual.

## Constraints

- `std::variant` (C++17, available since milestone 0002's C++20 raise) -
  no new runtime dependency.
- `std::vector<uint8_t>` is included as a `FieldValue` alternative
  specifically for fields like the Processor ID (8 raw bytes) or a UUID's
  raw form, where "stringify it" would be lossy or premature - a renderer
  decides how to represent raw bytes (hex string, base64, etc.), the
  field model doesn't decide for it.

## Tests

- `tests/unit/field_value_test.cpp` (new): construct a `NamedField` for
  each `FieldValue` alternative, confirm `std::get<T>`/`std::holds_alternative<T>`
  work as expected - a trivial but necessary smoke test before every
  entry type depends on this type.

## Success criteria

- [ ] `field_value.h` compiles standalone.
- [ ] `AbstractSMBiosEntry::get_fields()` is declared pure virtual (the
      codebase does not yet compile after this task alone - expected,
      per the Files section's note - confirmed resolved by Task 02).
