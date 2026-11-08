# 02 - OEM Strings, System Configuration Options, Firmware Language Information

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete

## Objective

Implement `OemStringsEntry` (`SMBiosHandler` value `11`, DSP0134 3.9.0
section 7.12), `SystemConfigurationOptionsEntry` (value `12`, section
7.13), and `FirmwareLanguageInformationEntry` (value `13`, section 7.14).

## Files

- **Create** `libsmbios/include/smbios/tables/oem_strings_entry.h` /
  `.cpp`: 0x04 Count (BYTE) - the only fixed field; content is entirely
  the trailing DMI string list.
- **Create**
  `libsmbios/include/smbios/tables/system_configuration_options_entry.h` /
  `.cpp`: 0x04 Count (BYTE) - identical shape to OEM Strings, different
  semantic meaning (jumper/switch configuration text, not OEM free-form
  text).
- **Create**
  `libsmbios/include/smbios/tables/firmware_language_information_entry.h` /
  `.cpp`:
  - 0x04 Installable Languages (BYTE)
  - 0x05 Flags (BYTE, bit 0 = abbreviated-format flag, 2.1+)
  - 0x06 Reserved (15 BYTEs)
  - 0x15 Current Language (BYTE, STRING)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support for all three.

## Functions and Classes

- `OemStringsEntry` - new. `get_string_count()`,
  `get_string_at(size_t index)` (1-based).
- `SystemConfigurationOptionsEntry` - new. Same shape as `OemStringsEntry`
  (`get_option_count()`, `get_option_at(size_t index)`) - deliberately not
  sharing a common base beyond `AbstractSMBiosEntry` itself, since the two
  types are only coincidentally identical in byte layout today and the
  spec defines them as unrelated structures (matching this project's
  existing precedent of never inventing a shared intermediate base class
  purely from current-shape similarity).
- `FirmwareLanguageInformationEntry` - new.
  `get_installable_language_count()`, `get_flags()` +
  `is_abbreviated_format()` (bit 0, 2.1+ - gate on `SMBiosVersion` the
  same way every other 2.1+-gated field in this codebase already does,
  e.g. `SystemSlotsEntry`'s Characteristics2), `get_current_language_index()`
  + `get_current_language_string()`, and
  `get_language_at(size_t index)` (0-based across the trailing language
  description strings, which start immediately after `Current Language`
  at string index 1 and run for `Installable Languages` entries).

## Constraints

- `OemStringsEntry`/`SystemConfigurationOptionsEntry`'s `get_string_at()`/
  `get_option_at()` must not be confused with `AbstractSMBiosEntry::dmi_string()`'s
  own 1-based indexing convention at the call site - document explicitly
  in each accessor's Doxygen comment which convention it follows (this
  task recommends matching `dmi_string()`'s 1-based convention exactly,
  since both accessors are thin wrappers over it).
- `FirmwareLanguageInformationEntry`'s abbreviated-vs-long string format
  (7.14's two trailing-string formats) only affects how the *CLI* should
  label the language strings for display (R7) - the entry class itself
  returns the raw strings unmodified either way; it does not attempt to
  parse the "ISO 639-1 | ISO 3166-1-alpha-2 | Encoding" long-format
  structure into separate fields (that is presentation, not structure
  parsing, matching this project's existing precedent of returning raw
  DMI strings and leaving further decomposition to callers where the spec
  itself does not define a fixed binary sub-layout).

## Tests

- `tests/unit/oem_strings_entry_test.cpp`,
  `tests/unit/system_configuration_options_entry_test.cpp`,
  `tests/unit/firmware_language_information_entry_test.cpp` (new):
  synthetic buffers with 0, 1, and 3+ strings (OEM Strings/Config
  Options); both abbreviated and long format, plus a pre-2.1 version
  guard case (Firmware Language Information).

## Success criteria

- [x] All three register and resolve through the factory.
- [x] Zero-string and multi-string cases both covered for the two
      string-list types.
- [x] Pre-2.1 version guard verified for Firmware Language Information's
      Flags field.
- [x] `smbios` CLI displays all three on real hardware where present
      (subject to the pre-existing, separately-tracked static-registration
      linking bug noted in this task's own assignment).
