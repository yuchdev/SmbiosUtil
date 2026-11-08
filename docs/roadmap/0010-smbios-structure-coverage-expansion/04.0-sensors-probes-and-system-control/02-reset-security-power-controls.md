# 02 - System Reset, Hardware Security, System Power Controls

**Parent story:** [README.md](README.md)
**Status:** ✅ Done

## Objective

Implement `SystemResetEntry` (`SMBiosHandler` value `23`), DSP0134 3.9.0
section 7.24; `HardwareSecurityEntry` (value `24`), section 7.25; and
`SystemPowerControlsEntry` (value `25`), section 7.26.

## Files

- **Create** `libsmbios/include/smbios/tables/system_reset_entry.h` /
  `.cpp`:
  - 0x04 Capabilities (BYTE, bitmask - this task spec's own source
    excerpt redacted this row's detail; confirm its exact bit layout
    against DSP0134 3.9.0's full Table 92 directly - it covers at least a
    "Status" enable bit and a "Boot Option"/"Boot Option on Limit"
    2-bit-field pair per the surrounding prose, but the precise bit
    positions must come from the spec table, not be guessed)
  - 0x05 Reset Count (WORD; `0xFFFF` = unknown)
  - 0x07 Reset Limit (WORD; `0xFFFF` = unknown)
  - 0x09 Timer Interval (WORD, minutes; `0xFFFF` = unknown)
  - 0x0B Timeout (WORD, minutes; `0xFFFF` = unknown)
- **Create** `libsmbios/include/smbios/tables/hardware_security_entry.h` /
  `.cpp`:
  - 0x04 Hardware Security Settings (BYTE, bitmask - this task spec's own
    source excerpt redacted this row too; confirm against the full
    Table 93 before implementing - it is known from the spec's general
    structure to encode 2-bit-field settings each for Power-on Password
    Status, Keyboard Password Status, Administrator Password Status, and
    Front Panel Reset Status)
- **Create**
  `libsmbios/include/smbios/tables/system_power_controls_entry.h` /
  `.cpp`:
  - 0x04 Next Scheduled Power-on Month (BYTE, BCD, 01h-12h)
  - 0x05 Next Scheduled Power-on Day-of-month (BYTE, BCD, 01h-31h)
  - 0x06 Next Scheduled Power-on Hour (BYTE, BCD, 00h-23h)
  - 0x07 Next Scheduled Power-on Minute (BYTE, BCD, 00h-59h)
  - 0x08 Next Scheduled Power-on Second (BYTE, BCD, 00h-59h)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all three.

## Functions and Classes

- `SystemResetEntry` - new. `get_capabilities()` + decoded accessors per
  the confirmed bit layout (e.g. `is_status_enabled()`,
  `get_boot_option()` + `get_boot_option_string()`,
  `get_boot_option_on_limit()` + `..._string()`, `is_watchdog_present()` -
  exact names pending the spec-table confirmation noted above),
  `get_reset_count()` + `is_reset_count_known()`, `get_reset_limit()` +
  `is_reset_limit_known()`, `get_timer_interval()` +
  `is_timer_interval_known()`, `get_timeout()` + `is_timeout_known()`.
- `HardwareSecurityEntry` - new. `get_settings()` plus decoded 2-bit-field
  accessors per the confirmed layout (e.g.
  `get_power_on_password_status()`, `get_keyboard_password_status()`,
  `get_administrator_password_status()`, `get_front_panel_reset_status()`,
  each likely a shared 4-value enum - Disabled/Enabled/Not Implemented/
  Unknown, per common SMBIOS 2-bit-status convention, but confirm the
  exact values against the spec rather than assuming).
- `SystemPowerControlsEntry` - new. Raw BCD accessors
  (`get_month_bcd()`, `get_day_bcd()`, `get_hour_bcd()`,
  `get_minute_bcd()`, `get_second_bcd()`) plus decimal convenience
  accessors (`get_month()`, `get_day()`, `get_hour()`, `get_minute()`,
  `get_second()`) via a shared private `bcd_to_decimal(uint8_t)` helper
  (7.26.1).

## Constraints

- Both `SystemResetEntry::get_capabilities()`'s bit layout and
  `HardwareSecurityEntry::get_settings()`'s bit layout **must** be
  confirmed against DSP0134 3.9.0's full Tables 92/93 before
  implementation - this task spec's own source excerpt had both rows
  redacted by the extraction tool that produced the markdown version of
  the spec; do not guess the exact bit positions from the surrounding
  prose summary alone.
- BCD values are not validated against their documented ranges by
  `bcd_to_decimal()` itself (e.g. a malformed `0xFF` byte in a BCD field
  is not a valid BCD digit pair) - `bcd_to_decimal()` should document
  that it performs the mechanical nibble-pair conversion
  (`(b >> 4) * 10 + (b & 0x0F)`) and leaves range validation, if any, to
  the caller/renderer.

## Tests

- `tests/unit/system_reset_entry_test.cpp`,
  `tests/unit/hardware_security_entry_test.cpp`,
  `tests/unit/system_power_controls_entry_test.cpp` (new): synthetic
  buffers per confirmed bit-layout value, unknown-sentinel cases for
  System Reset's 4 WORD fields, and at least one real BCD date/time
  decode-to-decimal case for System Power Controls.

## Success criteria

- [x] Capabilities/Settings bit layouts confirmed against the full spec
      table (not left as this task spec's placeholder names) - see
      `libsmbios/include/smbios/tables/system_reset_entry.h` and
      `hardware_security_entry.h`'s class doc comments for the confirmed
      Table 92/93 layouts.
- [x] All three types register and resolve through the factory
      (`SMBiosHandler` values 23, 24, 25).
- [x] BCD-to-decimal conversion verified by an explicit value assertion.
- [x] `smbios` CLI displays all three on real hardware where present (via
      `main.cpp`'s generic `render_to_description()` dispatch, same as
      every other entry type).
