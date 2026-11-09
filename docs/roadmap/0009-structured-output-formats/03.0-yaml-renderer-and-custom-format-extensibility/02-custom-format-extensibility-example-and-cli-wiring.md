# 02 - Custom-format extensibility example and CLI wiring

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Prove R5 with a real, separately-built CSV formatter living outside
`smbios`'s own source, and wire `--format` into the CLI (R6).

## Files

- **Create** `smbios_util/examples/csv_formatter.cpp`: a complete,
  minimal `IOutputFormatter` implementation (`entry.get_fields()` → one
  CSV row: `type,field_name,field_value`), registered via
  `REGISTER_OUTPUT_FORMATTER(CsvFormatter)` from a file that only
  `#include <smbios/output_formatter.h>` and
  `<smbios/abstract_smbios_entry.hpp>` - no access to any entry type's
  concrete header, proving the registry's public surface alone is enough
  to add a format.
- **Modify** `smbios_util/include/smbios_util/command_line_parser.hpp` /
  `.cpp`: add `("format,f", po::value<string>(&_format)->default_value("text"),
  "Output format: text|xml|json|yaml|csv, or any registered custom format")`.
- **Modify** `smbios_util/src/main.cpp`: look up
  `get_output_formatter(cmd_line_params.format())` before constructing
  `SMBios bios;` (per this story's Key constraints); on `nullptr`, print a
  clear error listing registered format names and exit non-zero;
  otherwise call the resolved formatter's `render()` per entry instead of
  the current hardcoded `render_to_description()` call.
- **Modify** `smbios_util/CMakeLists.txt` (or a new, separate example
  target): build `csv_formatter.cpp` into `smbios_util`'s binary (or a
  documented optional example target - decide which during
  implementation; the point being proven doesn't require it to ship by
  default, only to prove it *can* be built as a formatter with no
  `smbios` source changes).

## Functions and Classes

- `CsvFormatter` - new, outside `smbios/src/`.
- `main()` - modified to resolve a formatter by name instead of calling
  `render_to_description()` directly.

## Constraints

- `csv_formatter.cpp` must not `#include` any concrete entry-type header
  (`bios_information_entry.hpp`, etc.) - if it needs to, that's a sign
  `get_fields()`/`IOutputFormatter`'s public surface is insufficient, a
  Story 01.0/02.0 gap, not something to work around here.
- An unknown `--format` value exits with a clear, actionable message
  before any privileged SMBIOS read is attempted.

## Tests

- `tests/c_abi`-style integration test (C++ test, not C - this is the
  C++ CLI) invoking `smbios_util --format csv` and `--format bogus`,
  confirming the former produces valid CSV and the latter exits non-zero
  with a listing of valid format names.

## Success criteria

- [ ] `smbios_util --format csv` works, producing valid CSV, with
      `csv_formatter.cpp` never including a concrete entry-type header.
- [ ] `smbios_util --format bogus` fails fast with a clear message,
      before any SMBIOS read is attempted.
- [ ] `smbios_util` with no `--format` (or `--format text`) is
      byte-identical to its pre-milestone output.
