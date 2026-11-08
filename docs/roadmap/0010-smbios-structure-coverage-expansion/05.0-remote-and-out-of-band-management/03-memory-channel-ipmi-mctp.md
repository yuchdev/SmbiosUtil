# 03 - Memory Channel, IPMI Device Information, and MCTP Host Interface

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `MemoryChannelEntry` (`SMBiosHandler` value `37`), DSP0134
3.9.0 section 7.38; `IpmiDeviceInformationEntry` (value `38`), section
7.39; and `ManagementControllerHostInterfaceEntry` (value `42`), section
7.43 (3.2+ shape only - see this story's own Key constraints scope
cutoff).

## Files

- **Create** `libsmbios/include/smbios/tables/memory_channel_entry.h` /
  `.cpp`:
  - 0x04 Channel Type (BYTE, ENUM - see 7.38.1)
  - 0x05 Maximum Channel Load (BYTE)
  - 0x06 Memory Device Count `n` (BYTE)
  - 0x07 + `3*i` Memory Device `i` Load (BYTE), for `i` in `0..n-1`
  - 0x08 + `3*i` Memory Device `i` Handle (WORD), for `i` in `0..n-1`
- **Create**
  `libsmbios/include/smbios/tables/ipmi_device_information_entry.h` /
  `.cpp`:
  - 0x04 Interface Type (BYTE, ENUM - see 7.39.1)
  - 0x05 IPMI Specification Revision (BYTE, BCD - e.g. `0x10` = rev 1.0)
  - 0x06 I2C Target Address (BYTE)
  - 0x07 NV Storage Device Address (BYTE; `0xFF` = none)
  - 0x08 Base Address (QWORD; bit 0 = I/O-space flag, rest is the address)
  - 0x10/0x11 (this task spec's own excerpt redacted the row immediately
    before Interrupt Number; confirm the exact field(s) occupying this
    gap - likely a "Base Address Modifier" bit-field per common IPMI/
    SMBIOS implementations, but verify against the full Table 114 - before
    finalizing offsets)
  - 0x11 Interrupt Number (BYTE; `0x00` = unspecified/unsupported)
- **Create**
  `libsmbios/include/smbios/tables/management_controller_host_interface_entry.h` /
  `.cpp` (3.2+ shape):
  - 0x04 Interface Type (BYTE, ENUM - see 7.43.1)
  - 0x05 Interface Type Specific Data Length `N` (BYTE)
  - 0x06..0x06+N-1 Interface Type Specific Data (`N` BYTEs, raw - see
    Constraints)
  - 0x06+N Number of Protocol Records `X` (BYTE)
  - 0x07+N Protocol Records (variable-length array of Table 123's own
    `{Protocol Type, Protocol Type Specific Data Length, Protocol Type
    Specific Data}` records - confirm Table 123's exact layout against
    the spec directly before implementing the per-record walk)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all three.

## Functions and Classes

- `MemoryChannelEntry` - new. `get_channel_type()` +
  `get_channel_type_string()`, `get_maximum_channel_load()`,
  `get_device_count()` (reads the explicit count field at 0x06 - **not**
  derived from `Length`, per this story's own Key constraints note
  quoting the spec's explicit warning), `get_device_load_at(size_t
  index)`, `get_device_handle_at(size_t index)` (both 0-based, bounds
  checked against `get_device_count()` and independently against
  `get_entry_size()`).
- `IpmiDeviceInformationEntry` - new. `get_interface_type()` +
  `get_interface_type_string()`, `get_specification_revision_bcd()` +
  decimal convenience accessor (reusing the same `bcd_to_decimal()` idiom
  as Story 04.0's `SystemPowerControlsEntry`, or a shared helper if one
  already exists by the time this task lands), `get_i2c_target_address()`,
  `get_nv_storage_device_address()` + `has_nv_storage_device()` (sentinel
  check), `get_base_address()` + `is_base_address_io_space()` (bit 0),
  plus whatever field(s) the redacted 0x10/0x11 gap turns out to hold once
  confirmed, and `get_interrupt_number()` + `has_interrupt()` (sentinel
  check on 0x00).
- `ManagementControllerHostInterfaceEntry` - new.
  `get_interface_type()` + `get_interface_type_string()`,
  `get_interface_specific_data()` (raw bytes, length `N`),
  `get_protocol_record_count()`, and a per-record accessor walking the
  variable-length Protocol Records array (exact signature pending Table
  123's confirmed layout - likely
  `get_protocol_record_at(size_t index)` returning a small struct/pair of
  `{protocol_type, raw_data}` rather than a flat scalar, since each
  record is itself variable-length).

## Constraints

- `MemoryChannelEntry::get_device_count()` must read the explicit Memory
  Device Count field (0x06) and must **not** derive the count from
  `Length` - the spec's own text is explicit that `Length` "must not be
  used to determine the number of memory devices" specifically to allow
  future trailing fields after the load/handle list without breaking
  older parsers. This is the opposite derivation direction from Group
  Associations (14, Story 02.0), which has no count field and *must*
  derive from `Length` - do not copy that task's pattern here by mistake.
- Three redacted-row gaps are known in this task's own source excerpt
  (IPMI Device Information's 0x10/0x11 boundary; Management Controller
  Host Interface's Table 123 record layout) - both must be confirmed
  against the full DSP0134 3.9.0 document before implementation, not
  guessed from surrounding context.
- `ManagementControllerHostInterfaceEntry` targets the 3.2+ field layout
  only (R8 scope cutoff, documented in this story's own README) - no
  special-case branch for pre-3.2 OEM-type structures lacking the
  Interface Type Specific Data Length field.

## Tests

- `tests/unit/memory_channel_entry_test.cpp`,
  `tests/unit/ipmi_device_information_entry_test.cpp`,
  `tests/unit/management_controller_host_interface_entry_test.cpp` (new):
  Memory Channel with 0, 1, and 3+ devices (confirming the explicit count
  field, not `Length`, drives iteration); IPMI with both I/O-space and
  memory-mapped Base Address bit-0 cases and the two sentinel cases
  (NV Storage Device Address, Interrupt Number); MCTP Host Interface with
  0 and 2+ protocol records.

## Success criteria

- [ ] All three types register and resolve through the factory
      (`SMBiosHandler` values 37, 38, 42).
- [ ] Memory Channel's device count is verified to come from the explicit
      field, not `Length` (a test with a `Length` that would imply a
      different count if miscomputed should catch a regression here).
- [ ] Both redacted-row gaps confirmed against the full spec before
      implementation.
- [ ] `smbios` CLI displays all three on real hardware where present.
