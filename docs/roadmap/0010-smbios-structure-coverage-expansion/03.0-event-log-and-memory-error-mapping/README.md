# Story 03.0 - Event Log and Memory Error/Mapping Structures

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete
**Depends on:** Story 01.0 (`tables/` reorg)

## Scope

Implement System Event Log (15, DSP0134 3.9.0 section 7.16), 32-Bit
Memory Error Information (18, section 7.19), Memory Device Mapped Address
(20, section 7.21), and 64-Bit Memory Error Information (33, section
7.34).

## Tasks

| #  | Document                                                       | Status         | Blocks |
|----|---------------------------------------------------------------------|----------------|--------|
| 01 | [System event log](01-system-event-log.md)                            | ✅ Complete | 04     |
| 02 | [32-bit and 64-bit memory error information](02-memory-error-information.md) | ✅ Complete | 04     |
| 03 | [Memory device mapped address](03-memory-device-mapped-address.md)      | ✅ Complete | 04     |
| 04 | [Tests](04-tests.md)                                                 | ✅ Done     | -      |

Task 01 is split out alone: System Event Log (15) is the single most
structurally complex type in this milestone - a fixed header plus up to
three optional variable-length trailing sections (2.1+ supported-type
descriptor list; the event-log records themselves are a *separate*,
out-of-band non-volatile-storage area this structure only *points to* via
Access Method Address, not a trailing array within the structure itself)
- it does not belong batched with anything else. Task 02 pairs 32-bit
(18) and 64-bit (33) Memory Error Information because they are the same
fields at two different address widths (identical shape to
`MemoryDeviceEntry`'s own existing 32-bit/extended-size pattern, and the
same "legacy field sentinel -> wider field" idiom milestone 0004 already
used three times). Task 03 is Memory Device Mapped Address (20) alone
because, while its shape closely mirrors the already-implemented
`MemoryArrayMappedAddressEntry` (19), it adds two more handle fields
(Memory Device Handle, Memory Array Mapped Address Handle) and a
partition-row/interleave-position pair that type 19 does not have -
enough difference to warrant its own task rather than folding into Task
02.

## Key constraints

- Every new class follows the shape already established:
  `(const DMIHeader&, const SMBiosVersion&, const uint8_t*)` constructor,
  type check, `get_type()`/`render_to_description()`, `read_uN` +
  length-guard accessors.
- System Event Log's "Log Status," "Log Change Token," and "Access Method
  Address" fields describe the event log's *location and status*, not its
  *contents* - this class does not read, decode, or walk the actual
  non-volatile event-log records (7.16.4-7.16.6); that would require a
  second OS-level access mechanism (indexed I/O, memory-mapped, or GPNV)
  this codebase has no existing seam for. **Explicit scope cutoff (R8):**
  document this in the class's own doc comment, matching
  `SystemSlotsEntry`'s precedent of naming a scope cutoff rather than
  leaving it implicit.
- Both Memory Error Information types' three "unknown" sentinels
  (`8000 0000h` for addresses, `0000 0000h` for vendor syndrome) are
  genuine spec-defined sentinels, not a legacy-field-widening pattern -
  expose both the raw value and an `is_*_known()`/`is_*_unknown()`
  predicate per address field, rather than silently rendering the sentinel
  as if it were a real address.
- Memory Device Mapped Address (20)'s Starting/Ending Address ->
  Extended Starting/Ending Address widening (2.7+) is the fourth
  confirmed instance of the "`0xFFFFFFFF` sentinel -> read the wider
  field instead" idiom milestone 0004 already used three times - same
  pattern, no new design needed.

## Files or directories created or modified by this story

- `libsmbios/include/smbios/tables/system_event_log_entry.hpp` /
  `libsmbios/src/system_event_log_entry.cpp` (new)
- `libsmbios/include/smbios/tables/memory_error_information_32bit_entry.hpp` /
  `libsmbios/src/memory_error_information_32bit_entry.cpp` (new)
- `libsmbios/include/smbios/tables/memory_error_information_64bit_entry.hpp` /
  `libsmbios/src/memory_error_information_64bit_entry.cpp` (new)
- `libsmbios/include/smbios/tables/memory_device_mapped_address_entry.hpp` /
  `libsmbios/src/memory_device_mapped_address_entry.cpp` (new)
- `smbios/src/command_line_parser.cpp` / `main.cpp` - display support for
  all four.
