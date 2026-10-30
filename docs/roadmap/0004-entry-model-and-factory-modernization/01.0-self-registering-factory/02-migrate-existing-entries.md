# 02 - Migrate existing entries

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (registry and macro)

## Objective

Add one `REGISTER_SMBIOS_ENTRY(...)` line to each of the three existing
entry classes' headers, and delete the old hand-populated constructor body
in `smbios_entry_factory.cpp` ([smbios_entry_factory.cpp:8-13](/smbios/src/smbios_entry_factory.cpp))
along with the `boost::bind`/`boost::factory`/`boost::functional` includes
it required.

## Files

- **Modify** `smbios/include/smbios/bios_information_entry.h`: add
  `REGISTER_SMBIOS_ENTRY(SMBios::BIOSInformation, BiosInformationEntry);`
  at namespace scope (bottom of file, after the class definition).
- **Modify** `smbios/include/smbios/port_connection_entry.h`: add
  `REGISTER_SMBIOS_ENTRY(SMBios::PortConnection, PortConnectionEntry);`.
- **Modify** `smbios/include/smbios/memory_device_entry.h`: add
  `REGISTER_SMBIOS_ENTRY(SMBios::MemoryDevice, MemoryDeviceEntry);`.
- **Modify** `smbios/src/smbios_entry_factory.cpp`: delete the
  `SMBiosEntryFactory::SMBiosEntryFactory()` constructor body entirely (no
  constructor is needed once registration is static-init-driven); remove
  `#include <boost/functional/factory.hpp>` and `#include <boost/bind.hpp>`.
- **Modify** `vcpkg.json` (milestone 0001): remove `boost-functional` and
  `boost-bind` from the dependency list, if nothing else in the codebase
  still uses them (confirm with a repo-wide grep before removing).

## Functions and Classes

- `SMBiosEntryFactory` - default constructor removed (no longer declared
  if the class has no other state; confirm during implementation whether
  an explicit default is still needed for ABI/API stability, or whether
  deleting it entirely is clean).

## Constraints

- `smbios_entry_factory.h`'s `#include` list for the three concrete entry
  headers (`bios_information_entry.h`, `port_connection_entry.h`,
  `memory_device_entry.h`) can be removed once the factory header no
  longer needs to know about concrete entry types at all - the whole point
  of self-registration is that the factory header stops listing every
  entry class. Confirm this and update the include list accordingly.

## Tests

- Extend `smbios/test/functional_test/func_tests.cpp` (or the new
  `smbios_entry_factory_test.cpp`) to confirm `SMBiosEntryFactory::create()`
  still returns the correct concrete type for all three existing
  `SMBiosHandler` values, exactly as before this story.

## Success criteria

- [ ] `grep -rn "boost::bind\|boost::factory\|boost/functional\|boost/bind"
      smbios/` returns no matches.
- [ ] `SMBiosEntryFactory::create()` returns the correct concrete type for
      `BIOSInformation`, `PortConnection`, and `MemoryDevice` - verified by
      test, not just by inspection.
- [ ] Existing functional test suite passes unmodified on real hardware.
