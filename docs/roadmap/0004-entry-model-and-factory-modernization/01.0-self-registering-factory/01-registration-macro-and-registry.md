# 01 - Registration macro and registry

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Define the registry and a `REGISTER_SMBIOS_ENTRY(HandlerValue, ClassName)`
macro that registers `ClassName` against `SMBios::SMBiosHandler::HandlerValue`
at static-init time, with no edits to any shared factory file required to
add a new entry type.

## Files

- **Modify** `smbios/include/smbios/smbios_entry_factory.h`:
  ```cpp
  #pragma once
  #include <cstdint>
  #include <functional>
  #include <memory>

  namespace smbios {

  class AbstractSMBiosEntry;
  struct DMIHeader;
  struct SMBiosVersion;

  using EntryCreator = std::function<std::unique_ptr<AbstractSMBiosEntry>(const DMIHeader&, const SMBiosVersion&)>;

  /// @brief Registers an entry creator against a SMBiosHandler value; asserts on duplicate registration.
  void register_smbios_entry(uint8_t handler_value, EntryCreator creator);

  class SMBiosEntryFactory {
  public:
      /// @brief Create a concrete SMBIOS entry instance via the self-registered creator, or nullptr if unregistered.
      std::unique_ptr<AbstractSMBiosEntry> create(const DMIHeader&, const SMBiosVersion&);
  };

  } // namespace smbios

  #define REGISTER_SMBIOS_ENTRY(handler_value, class_name)                                   \
      namespace {                                                                            \
      const bool class_name##_registered = [] {                                              \
          smbios::register_smbios_entry(static_cast<uint8_t>(handler_value),                 \
              [](const smbios::DMIHeader& h, const smbios::SMBiosVersion& v) {               \
                  return std::make_unique<class_name>(h, v);                                  \
              });                                                                             \
          return true;                                                                        \
      }();                                                                                     \
      }
  ```
- **Modify** `smbios/src/smbios_entry_factory.cpp`: implement
  `register_smbios_entry` over a function-local `static std::map<uint8_t,
  EntryCreator>&` (Meyers-singleton style, to sidestep static-init order
  across translation units), and `SMBiosEntryFactory::create` to look it
  up.

## Functions and Classes

- `smbios::register_smbios_entry(uint8_t, EntryCreator)` - new free
  function.
- `smbios::EntryCreator` - new type alias (`std::function`).
- `REGISTER_SMBIOS_ENTRY(handler_value, class_name)` - new macro.
- `SMBiosEntryFactory::create` - same signature, new implementation
  (registry lookup instead of a hand-populated map).

## Constraints

- Duplicate registration (same `handler_value` registered twice) must
  `assert`/throw at static-init time, per this story's Key constraints -
  write the test for this (Task 03) against a deliberately-duplicated
  registration in a throwaway test translation unit, not against
  production entry classes.
- No change to `AbstractSMBiosEntry` in this task.

## Tests

- `tests/unit/smbios_entry_factory_test.cpp` (new): register a fake test
  entry type under an unused `SMBiosHandler`-shaped value, confirm
  `create()` returns an instance of it; confirm `create()` returns
  `nullptr` for a truly unregistered value; confirm a duplicate
  registration triggers the fail-fast path.

## Success criteria

- [ ] `register_smbios_entry`/`REGISTER_SMBIOS_ENTRY` work from a test
      translation unit with no entry classes touched yet.
- [ ] `SMBiosEntryFactory::create()` keeps its exact public signature.
- [ ] Duplicate registration is caught, not silently overwritten.
