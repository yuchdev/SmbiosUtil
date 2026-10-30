# Story 01.0 - Self-Registering Factory

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started

## Scope

Replace `SMBiosEntryFactory`'s `std::map<uint8_t, boost::function<...>>`
populated by hand in its constructor with a module-scope registry
populated by each entry class at static-init time (`REGISTER_SMBIOS_ENTRY`
macro, modeled on the registration-decorator pattern), built on
`std::function` instead of `boost::function`/`boost::bind`/`boost::factory`.
Migrate the three existing entries (`BiosInformationEntry`,
`PortConnectionEntry`, `MemoryDeviceEntry`) onto it before Story 02.0 adds
eight more.

## Tasks

| #  | Document                                                    | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [Registration macro and registry](01-registration-macro-and-registry.md) | ⬜ Not started | 02     |
| 02 | [Migrate existing entries](02-migrate-existing-entries.md)       | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                              | ⬜ Not started | -      |

## Key constraints

- `SMBiosEntryFactory::create(const DMIHeader&, const SMBiosVersion&)`
  keeps its exact public signature and return type
  (`std::unique_ptr<AbstractSMBiosEntry>`) - callers (`smbios_util`'s CLI)
  are unaffected.
- Static-init registration order across translation units is unspecified
  in C++ - the registry itself (a function-local `static` map, not a
  namespace-scope global) must be safe regardless of registration order,
  same hazard class the project's own earlier `postprocess`-style registry
  patterns in sibling ecosystems already solve this way.
- Duplicate registration of the same `SMBiosHandler` value is a programming
  error, not silently ignored or silently overwritten - fail fast (an
  assertion or a thrown exception at static-init time is acceptable; a
  silent `operator[]` overwrite is not).

## Files or directories created or modified by this story

- `smbios/include/smbios/smbios_entry_factory.h` / `.cpp` - rebuilt.
- `smbios/include/smbios/bios_information_entry.h`,
  `port_connection_entry.h`, `memory_device_entry.h` - each gains one
  `REGISTER_SMBIOS_ENTRY(...)` line.
