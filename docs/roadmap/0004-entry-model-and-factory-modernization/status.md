# Milestone 0004 - Entry Model and Factory Modernization - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                        | Status         | Tests |
|-------|-------------------------------|----------------|-------|
| 01.0  | Self-registering factory      | ✅ Complete | smbios_unit_test pass (41 cases incl. SMBiosEntryFactoryUnitTests: 5) |
| 02.0  | Expand structure coverage      | ✅ Complete | smbios_unit_test pass (139 cases) |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Depends on milestone 0003's accessor pattern (`read_uN` + length guards)
  landing first, so every entry class this milestone adds is written
  against the final pattern rather than the old `reinterpret_cast`
  technique.
- The eight new structure types (Story 02.0) are exactly the remaining
  members of `SMBios::SMBiosHandler`
  ([smbios.h:104-118](/libsmbios/include/libsmbios/smbios.hpppp)) that
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
- **2026-11-07: milestone 0004 Story 01.0 confirmed by real, green,
  5-job CI** (`https://github.com/yuchdev/SmbiosSdk/actions/runs/42274156013`:
  `build` × ubuntu/macos/windows, `sanitizers` × ubuntu/macos).
- **2026-11-07: Story 02.0 Task 01 complete** - `SystemInformationEntry`
  (type 1) and `SystemEnclosureEntry` (type 3) implemented, every offset
  cross-checked against `c-versions/dmidecode_linux`'s own `case 1`/
  `case 3` parsing and confirmed exact, no corrections needed. The
  UUID field's well-known mixed-endian quirk (first 3 fields
  little-endian, swapped to print in big-endian hex; last 2 fields
  printed as-is; the swap itself only applied at SMBIOS version >= 2.6,
  confirmed via `dmi_system_uuid()` in the reference) is implemented
  and tested against four cases: a normal UUID at >= 2.6 (swap
  applied), the identical raw bytes at < 2.6 (NOT swapped - the
  regression a copy-paste-without-the-version-check would produce),
  and both sentinel values (all-`0xFF` → "Not Present", all-`0x00` →
  "Not Settable"). `SystemEnclosureEntry`'s SKU number field sits at a
  genuinely variable, runtime-computed offset
  (`0x15 + contained_element_count * record_length`) - implemented and
  tested with a non-zero count/record-length fixture proving the
  offset is actually computed, not assumed fixed, plus a one-byte-short
  truncation case proving it still defaults safely rather than reading
  out of bounds. Contained-elements array itself: only the raw
  count/record-length accessors implemented, not a per-element
  type/minimum/maximum decode - a deliberate, documented scope
  narrowing (not named as a hard requirement in this task's own
  objective). `smbios_util`'s CLI needed no changes - `main.cpp`
  already dispatches `render_to_description()` polymorphically through
  the factory-returned base pointer, confirmed by direct read rather
  than assumed. Verified independently: direct file read of the UUID
  formatter and the variable-offset SKU logic, a real local macOS
  rebuild, and a full `ctest` run (`smbios_unit_test` 60/60 cases).
- **2026-11-07: Story 02.0 Task 02 complete** - `ProcessorInformationEntry`
  (type 4, the richest revision history of any type this project
  implements) and `CacheInformationEntry` (type 7). Four distinct
  `0xFF`/`0xFE`-sentinel-to-widened-field relationships implemented and
  each independently tested without-sentinel/with-sentinel/length-
  insufficient-fallback: `core_count` (byte `0x23` → `u16` at `0x2A`),
  `core_enabled` (`0x24` → `0x2C`), `thread_count` (`0x25` → `0x2E`),
  and - not explicitly named in this task's own objective, but real and
  flagged as mandatory during briefing - `processor_family` (`0x06`
  sentinel value `0xFE`, not `0xFF` → `u16` at `0x28`), confirmed against
  `dmi_processor_family()`'s own `data[0x06] == 0xFE && h->length >= 0x2A`
  check. Voltage's two-branch bit-7 encoding (explicit volts vs. a
  multi-bit-settable legacy bitmask of 5.0V/3.3V/2.9V) and cache size's
  granularity-flag encoding (bit 31/15: 64KB vs 1KB units) both
  independently re-derived correctly, confirmed by direct code read
  against `dmi_processor_voltage()`/`dmi_cache_size_2()`. Cache's
  legacy-vs-widened size-field selection (a length-threshold check, not
  an `0xFF` sentinel, but the same spirit) tested with the legacy and
  widened fields deliberately holding different values, proving which
  one actually wins rather than merely returning a plausible number.
  Scope calls made and documented: `processor_id` exposed raw (no
  x86 CPUID/ARM MIDR architecture-specific decode); the full 234-entry
  processor-family and 80-entry upgrade string tables keyed by numeric
  code in a map (many real DMTF names, e.g. `"80286"`, aren't valid C++
  identifiers); two manufacturer-string-dependent ambiguous family codes
  (`0x30`, `0xBE`) not reproduced, since they need a second unrelated
  field and are a presentation nuance layered on top of the sentinel
  concern this task actually cares about. Verified independently: direct
  file read of all four sentinel paths and both encoding decodes, a real
  local macOS rebuild under `-Wall -Wextra -Werror`, and a full `ctest`
  run (`smbios_unit_test` 104/104 cases).
- **2026-11-07: Story 02.0 Task 03 complete - all eight new structure
  types now implemented.** `SystemSlotsEntry` (type 9, scoped through
  segment/bus/device-function number per this task's own objective -
  the later 3.2+ peer-device array at a second layer of computed
  offset is deliberately out of scope, documented), `PhysicalMemoryArrayEntry`
  (type 16, `0x80000000` capacity sentinel), `MemoryArrayMappedAddressEntry`
  (type 19, `0xFFFFFFFF` address sentinel - a genuinely different
  sentinel value than every other case this milestone implemented,
  confirmed not conflated with `0x80000000` by direct code read), and
  `SystemBootInformationEntry` (type 32, a variable-length trailing
  payload exposed as a raw byte range, exempt from the usual fixed-
  offset `read_uN` treatment per this task's own stated exception).
  **The implementing agent caught and corrected a real error in this
  task's own briefing**: the boot-status enum table given in the brief
  (21 entries) does not exist in either `c-versions/dmidecode_linux` or
  `dmidecode_macos` - independently re-verified here by direct read of
  `dmi_system_boot_status()`, confirming the real table has only 9
  named codes (0-8) plus OEM-specific (128-191) and Product-specific
  (192+) ranges, exactly matching DMTF 7.33.1. The real 9-entry table
  is what's implemented. Verified independently: direct file read of
  both sentinel implementations (confirming the two distinct sentinel
  values aren't conflated) and the corrected boot-status table, a real
  local macOS rebuild, and a full `ctest` run (`smbios_unit_test`
  138/138 cases).
- **2026-11-07: Story 02.0 Task 04 complete, milestone exit gate met.**
  Added one consolidated test
  (`CreateResolvesEveryRealSMBiosHandlerExceptEndOfTableTestCase`,
  `tests/unit/smbios_entry_factory_test.cpp`) proving this story's own
  closing success criterion directly - every one of the 11 real
  `SMBios::SMBiosHandler` values resolves to a non-null, correctly-
  dispatched entry through `SMBiosEntryFactory::create()`, and
  `EndOfTable` (the table's own terminator, never constructed as an
  entry) correctly resolves to `nullptr` - rather than leaving this
  claim implied by eleven separate per-type test files. `smbios_unit_test`
  139/139 cases. **Not independently confirmable from this sandbox:**
  the task's own "`smbios_util` run against real hardware prints a
  complete report" criterion needs a real machine with a native SMBIOS
  source - every CI runner this project has used throughout its history
  has the same no-root/no-native-source limitation as this Mac, so this
  specific criterion has never been (and likely cannot be) confirmed by
  CI either; it is honestly left open rather than claimed as verified.

## Milestone 0004 - closing summary

Both stories are complete, confirmed by real CI through Story 01.0
(5-job green run) and locally through Story 02.0 (CI confirmation
pending the push that closes this milestone). `SMBiosEntryFactory`'s
hand-wired `boost::bind`/`boost::factory` constructor is gone entirely,
replaced by `REGISTER_SMBIOS_ENTRY`-based self-registration against a
module-scope registry - adding entry #12 in a future milestone touches
one new file, not a shared factory constructor. All eight previously-
unimplemented `SMBios::SMBiosHandler` structure types
(`SystemInformation` through `SystemBootInformation`) are now real,
tested classes built directly on milestone 0003's `read_uN` +
length-guard pattern, with every offset cross-checked against
`c-versions/dmidecode_linux`'s real parsing rather than derived from
memory of the DMTF spec. Along the way: one genuinely new formatting
concern (the UUID field's mixed-endian quirk), five confirmed
sentinel-to-widened-field relationships across three different
sentinel values (`0xFF`, `0xFE`, `0x80000000`, `0xFFFFFFFF` - the
`0xFF`/`0x80000000` pair appearing twice each, at different
offsets/structures), two non-trivial bit-encoding schemes (processor
voltage, cache-size granularity), and one real error in this
conversation's own task briefing (a fabricated 21-entry boot-status
table) caught and corrected against the real reference rather than
shipped unverified.
