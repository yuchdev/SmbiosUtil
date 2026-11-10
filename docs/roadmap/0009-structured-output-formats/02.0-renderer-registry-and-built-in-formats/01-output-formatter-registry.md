# 01 - Output formatter registry

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C2**: `IOutputFormatter`, the self-registering
registry, and the `REGISTER_OUTPUT_FORMATTER` macro. This is the same
shape milestone 0004 Story 01.0 built for entry types, applied to output
formats.

**Multi-entry document framing was RATIFIED on 2026-11-10 as option (a)**
(see `status.md` Notes & decisions). In addition to the C2 members below,
`IOutputFormatter` declares three non-pure virtual hooks:
`virtual std::string begin_document() const { return {}; }`,
`virtual std::string entry_separator() const { return {}; }` and
`virtual std::string end_document() const { return {}; }`.
The output is `begin_document()`, then each `render(entry)` with
`entry_separator()` between consecutive entries, then `end_document()`.
Task 02 and Story 03.0 build against this interface.

## Files

- **Create** `libsmbios/include/libsmbios/output_formatter.hpp` (Doxygen on
  every public declaration, mirroring `smbios_entry_factory.hpp`'s comment
  style):
  ```cpp
  #pragma once
  #include <memory>
  #include <string>
  #include <vector>

  namespace smbios {

  class AbstractSMBiosEntry;

  class IOutputFormatter {
  public:
      virtual ~IOutputFormatter() = default;
      virtual std::string render(const AbstractSMBiosEntry& entry) const = 0;
      virtual std::string format_name() const = 0;
  };

  /// @throws std::logic_error if formatter->format_name() is already registered.
  void register_output_formatter(std::unique_ptr<IOutputFormatter> formatter);
  /// @return the registered formatter, or nullptr if `name` is unregistered.
  const IOutputFormatter* get_output_formatter(const std::string& name);
  /// @return every registered format name, sorted ascending.
  std::vector<std::string> registered_output_formatter_names();

  } // namespace smbios

  /// Place exactly one invocation at namespace scope in the formatter's own
  /// .cpp (never its header - same duplicate-registration reasoning as
  /// REGISTER_SMBIOS_ENTRY).
  #define REGISTER_OUTPUT_FORMATTER(class_name)                                    \
      namespace {                                                                   \
      const bool class_name##_formatter_registered = [] {                           \
          smbios::register_output_formatter(std::make_unique<class_name>());        \
          return true;                                                               \
      }();                                                                           \
      }
  ```
  The header forward-declares `AbstractSMBiosEntry`. A formatter's `.cpp`
  includes `<libsmbios/abstract_smbios_entry.hpp>` itself.
- **Create** `libsmbios/src/output_formatter.cpp` containing the registry
  implementation. The registry is a function-local
  `static std::map<std::string, std::unique_ptr<IOutputFormatter>>&`
  (a Meyers singleton), matching `smbios_entry_factory.cpp`'s
  static-init-order-safety reasoning.

## Functions and Classes

- `smbios::IOutputFormatter`, `register_output_formatter`,
  `get_output_formatter`, `registered_output_formatter_names`,
  `REGISTER_OUTPUT_FORMATTER` - new.

## Constraints

- **Duplicate names throw.** Registering a `format_name()` that is already
  registered is a programming error. It throws `std::logic_error`, exactly
  matching `register_smbios_entry()`'s documented rule, and never silently
  overwrites.
- **Unknown names return `nullptr`.** `get_output_formatter()` returns
  `nullptr` (it does not throw) for an unregistered name. This is
  deliberate: the `smbios` CLI (Story 03.0) turns that into a clear
  command-line error listing `registered_output_formatter_names()`, which
  is a cleaner user experience than an uncaught exception's stack unwind.
- **Names are case-sensitive and lowercase.** The built-in names are
  `text`, `xml`, `json` and `yaml`.
- *Amended 2026-11-10 (security-auditor INFO-6):* **framing and errors
  carry no firmware data.**
  - `begin_document()`, `entry_separator()` and `end_document()` take no
    entry and return only fixed, formatter-owned text, such as the XML
    declaration and root element, or the JSON `[`/`,`/`]`. They therefore
    cannot be a firmware-injection point.
  - The concatenated output must be one well-formed document for zero, one
    and many entries. [Task 03](03-tests.md) case 9 asserts this.
  - The `std::logic_error` raised on a duplicate name, and every other
    registry or formatter diagnostic, names only the format name or the
    field name. It never includes a field value or a raw DMI string.
    Source: [threat model INFO-6](../../../security/2026-11-10-structured-field-model.md).

## Tests

- `tests/unit/output_formatter_registry_test.cpp` (new,
  `BOOST_AUTO_TEST_SUITE(OutputFormatterRegistryUnitTests)`). It mirrors
  `tests/unit/smbios_entry_factory_test.cpp`'s structure:
  - Register a fake test formatter at namespace scope via
    `REGISTER_OUTPUT_FORMATTER` and confirm that lookup by name succeeds.
  - Confirm that an unregistered name returns `nullptr`.
  - Confirm that `registered_output_formatter_names()` is sorted and
    contains the fake.
  - Confirm that a direct duplicate `register_output_formatter()` call
    throws `std::logic_error`.

## Success criteria

- [ ] `begin_document()`/`entry_separator()`/`end_document()` exist with
      empty-string defaults, and a unit test proves the defaults.
- [ ] The registry behaves identically in shape and failure modes to the
      entry-type registry. Verify this with a side-by-side read of both
      test files, not just by inspecting the code.
- [ ] *Amended 2026-11-10 (security-auditor INFO-6):* the framing hooks
      return fixed text only, and no diagnostic includes a field value.
- [ ] `smbios_unit_test` is green.
