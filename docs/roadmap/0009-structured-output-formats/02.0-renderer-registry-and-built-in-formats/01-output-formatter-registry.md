# 01 - Output formatter registry

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C2**: `IOutputFormatter`, the self-registering
registry, and the `REGISTER_OUTPUT_FORMATTER` macro - the exact same
shape milestone 0004 Story 01.0 built for entry types, applied to output
formats.

## Files

- **Create** `smbios/include/smbios/output_formatter.hpp`:
  ```cpp
  #pragma once
  #include <memory>
  #include <string>
  #include <smbios/abstract_smbios_entry.hpp>

  namespace smbios {

  class IOutputFormatter {
  public:
      virtual ~IOutputFormatter() = default;
      virtual std::string render(const AbstractSMBiosEntry& entry) const = 0;
      virtual std::string format_name() const = 0;
  };

  void register_output_formatter(std::unique_ptr<IOutputFormatter> formatter);
  const IOutputFormatter* get_output_formatter(const std::string& name);

  } // namespace smbios

  #define REGISTER_OUTPUT_FORMATTER(class_name)                                    \
      namespace {                                                                   \
      const bool class_name##_formatter_registered = [] {                           \
          smbios::register_output_formatter(std::make_unique<class_name>());        \
          return true;                                                               \
      }();                                                                           \
      }
  ```
- **Create** `smbios/src/output_formatter.cpp`: the registry
  implementation, a function-local `static std::map<std::string,
  std::unique_ptr<IOutputFormatter>>&` (Meyers-singleton, matching
  milestone 0004's entry registry's own static-init-order-safety
  reasoning).

## Functions and Classes

- `smbios::IOutputFormatter`, `register_output_formatter`,
  `get_output_formatter`, `REGISTER_OUTPUT_FORMATTER` - new.

## Constraints

- Duplicate registration of the same `format_name()` is a programming
  error, fail-fast (assert/throw), exactly matching milestone 0004's
  entry-registry precedent (`smbios_entry_factory`'s equivalent rule) -
  do not silently overwrite.
- `get_output_formatter()` returning `nullptr` (not throwing) on an
  unregistered name is deliberate - `smbios_util`'s CLI (Story 03.0)
  turns that into a clear command-line error message, which is a cleaner
  user experience than an uncaught exception's stack unwind.

## Tests

- `tests/unit/output_formatter_registry_test.cpp` (new): register a fake
  test formatter, confirm lookup by name succeeds; confirm an
  unregistered name returns `nullptr`; confirm duplicate registration is
  caught.

## Success criteria

- [ ] The registry behaves identically in shape and failure modes to
      milestone 0004's entry-type registry (verified by a side-by-side
      read of both test files, not just by inspection).
