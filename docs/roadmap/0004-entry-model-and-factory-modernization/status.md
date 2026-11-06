# Milestone 0004 - Entry Model and Factory Modernization - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                        | Status         | Tests |
|-------|-------------------------------|----------------|-------|
| 01.0  | Self-registering factory      | ✅ Complete | smbios_unit_test pass (41 cases incl. SMBiosEntryFactoryUnitTests: 5) |
| 02.0  | Expand structure coverage      | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Depends on milestone 0003's accessor pattern (`read_uN` + length guards)
  landing first, so every entry class this milestone adds is written
  against the final pattern rather than the old `reinterpret_cast`
  technique.
- The eight new structure types (Story 02.0) are exactly the remaining
  members of `SMBios::SMBiosHandler`
  ([smbios.h:104-118](/smbios/include/smbios/smbios.h)) that
  `SMBiosEntryFactory::create()` currently returns `nullptr` for:
  `SystemInformation`, `SystemEnclosure`, `ProcessorInformation`,
  `CacheInformation`, `SystemSlots`, `PhysicalMemoryArray`,
  `MemoryArrayMappedAddress`, `SystemBootInformation`.
- **2026-11-07, correction found at `implement-milestone` cold-start R3**
  (direct read of the real `smbios_entry_factory.h`/`.cpp`, not the
  spec's example code): Story 01.0's own "Key constraints" and Task 01's
  literal code sample both describe/assume
  `create(const DMIHeader&, const SMBiosVersion&)` - a 2-argument
  signature. That was accurate when this milestone's spec was written,
  but milestone 0003's Story 03.0 Task 02 (bounds-checked string
  parsing) has since added a required third `const uint8_t* table_end`
  parameter to `create()`, `AbstractSMBiosEntry`'s constructor, and every
  concrete entry constructor, threaded through ~24 real call sites. The
  spec's "keeps its exact public signature" intent clearly means "don't
  break the real current callers" - so the new self-registration
  mechanism (`EntryCreator`, `REGISTER_SMBIOS_ENTRY`, `create()`) must be
  built against the real, current 3-argument signature, not the spec's
  stale 2-argument example. Recorded here so Story 01.0's tasks don't
  silently regress milestone 0003's own fix.
- **2026-11-07, Task 01 implementation note:** Task 01's own file
  (`01-registration-macro-and-registry.md`) and Task 02's file
  (`02-migrate-existing-entries.md`) both suggest placing the new
  `REGISTER_SMBIOS_ENTRY(...)` invocation in each entry's *header*
  (`bios_information_entry.h`/`port_connection_entry.h`/
  `memory_device_entry.h`). That placement was not used: each of those
  headers is `#include`d both by its own `.cpp` and by a separate
  `tests/unit/*_entry_test.cpp` that links into the same
  `smbios_unit_test` binary, so a header-placed invocation would run its
  static initializer once per translation unit and immediately trip the
  new duplicate-registration guard at test-binary startup. The macro
  invocation was placed in each entry's own `.cpp` file instead
  (`bios_information_entry.cpp`/`port_connection_entry.cpp`/
  `memory_device_entry.cpp`), which only one TU per binary ever compiles.
  Because achieving a fully-green test suite within Task 01 required the
  old `boost::bind`-populated constructor and the new registry to not
  coexist cleanly (the new `SMBiosEntryFactory` has no `entries_factory_`
  member to merge into), Task 01 also carried out Task 02's migration of
  the three existing entries and Task 02/03's `vcpkg.json` cleanup in the
  same pass: `boost::bind`/`boost::factory`/`boost/functional`+`boost/bind`
  are gone from `smbios/`, and `boost-functional`/`boost-bind` are removed
  from `vcpkg.json` (confirmed via repo-wide grep that nothing else in
  `smbios/`, `smbios_util/`, or `tests/` referenced them). Fail-fast
  mechanism chosen: `register_smbios_entry` throws `std::logic_error` on
  a duplicate `handler_value`, rather than `assert(false)` - this lets
  `tests/unit/smbios_entry_factory_test.cpp` exercise the duplicate path
  with `BOOST_CHECK_THROW` inside a normal (non-static-init) test-case
  call, which an `assert`/`abort()` path could not be without a
  death-test harness this project's Boost.Test setup does not have.
- **2026-11-07: Story 01.0 closed.** Independently re-verified Task 01's
  work by direct file read (registry/macro design, macro placement in
  `.cpp` files not headers, zero lingering `boost::bind`/`boost::factory`
  anywhere, `vcpkg.json` cleaned) and a real local macOS rebuild. Found
  one real gap against Task 02/03's own stated success criterion
  ("`create()` returns the correct concrete type for `BIOSInformation`,
  `PortConnection`, and `MemoryDevice` - verified by test, not just by
  inspection"): only `PortConnection` was actually exercised through
  `create()` itself (`unit_tests.cpp`); `BiosInformationEntry`/
  `MemoryDeviceEntry` were only ever constructed directly in their own
  test files. Closed by adding `CreateReturnsBiosInformationEntryTestCase`/
  `CreateReturnsMemoryDeviceEntryTestCase` to
  `tests/unit/smbios_entry_factory_test.cpp`, each resolving the real
  production type through the real factory against a minimal 4-byte
  buffer. Full suite re-verified: `smbios_unit_test` 41/41,
  `smbios_mock_test` pass; `smbios_integration_test` fails cleanly on
  real assertions (no root/hardware), the unchanged baseline. Real
  3-OS/sanitizer CI confirmation awaits the next push.
