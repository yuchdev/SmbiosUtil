# 02 - Custom-format extensibility example and CLI wiring

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Prove R5 with a real, separately built CSV formatter that lives outside
`libsmbios`'s source, and wire `--format` into the `smbios` CLI (R6).

## Files

- **Create** `smbios/examples/csv_formatter.cpp`. This is a complete,
  minimal `IOutputFormatter` implementation: `entry.get_fields()` produces
  CSV rows of `type,field_name,field_value`, with RFC 4180 quoting.
  - Raw bytes go through `hex_encode()` from
    `<libsmbios/formatters/formatter_escaping.hpp>`.
  - It is registered via `REGISTER_OUTPUT_FORMATTER(CsvFormatter)`.
  - It includes only `<libsmbios/output_formatter.hpp>`,
    `<libsmbios/abstract_smbios_entry.hpp>` and
    `<libsmbios/formatters/formatter_escaping.hpp>`. It has no access to
    any concrete entry header, which proves the registry's public surface
    alone is enough to add a format.
- **Create** `smbios/examples/CMakeLists.txt`. It compiles
  `csv_formatter.cpp` directly into the `smbios` executable, using either
  `target_sources(smbios PRIVATE ...)` or an `OBJECT` library linked into
  `smbios`. Both are valid under the root `cmake_minimum_required(VERSION 3.13)`.
  It is never built through a static archive, so the static initializer
  runs without whole-archive flags. Apply `${SMBIOS_COMPILE_OPTIONS}` and
  `${SMBIOS_WARNING_OPTIONS}`, as `smbios/src/CMakeLists.txt` does.
- **Modify** `smbios/CMakeLists.txt` and add `add_subdirectory(examples)`
  after `add_subdirectory(src)`. This is build wiring only, per the Story
  README's R5 note.
- **Modify** `smbios/include/smbios/command_line_parser.hpp` and
  `smbios/src/command_line_parser.cpp`.
  - Add `std::string _format` with a `format()` accessor, and register the
    option:
    `("format,f", po::value<string>(&_format)->default_value("text"), <help>)`.
  - Build `<help>` from `registered_output_formatter_names()`, for example
    `"Output format, one of: csv, json, text, xml, yaml"`. Do not hard-code
    the list. The registry is fully populated before `main()` runs.
  - `-f` does not collide with today's short options (`-h`, `-v`, `-m`,
    `-r`, `-d`).
  - `--format` is not added to the `_help`/`_version`/`_memory_scan`
    mutually-exclusive list.
- **Modify** `smbios/src/main.cpp`:
  - In the existing parameter-handling block, before `SMBios bios;`,
    resolve `get_output_formatter(cmd_line_params.format())`. On `nullptr`,
    print `Unknown output format '<name>'. Registered formats: <comma-joined
    registered_output_formatter_names()>` to `std::cerr` and
    `return EXIT_FAILURE`.
    - This deliberately does not go through `usage()`, which exits 0
      today. That would make a bad `--format` look successful.
  - In the entry loop, replace `entry->render_to_description()` with the
    resolved formatter's `render(*entry)`.
  - For `text`, all surrounding output stays exactly as it is today:
    - the `DMI version`/`Table size` lines;
    - `bios.render_to_description()`;
    - the `Header ID = N` lines;
    - the trailing `'\n'`.
  - For other formats, frame the output per Story 02.0 Task 01's recorded
    framing decision.
  - `--dump-file` behaviour is unchanged: it writes the raw table and
    returns before any entry is rendered, so `--format` has no effect
    there.
  - `--read-file` remains the existing `TODO` and is out of scope.
- **Modify** `tests/integration/CMakeLists.txt` and add CTest CLI smoke
  tests (see Tests).

## Functions and Classes

- `CsvFormatter` - new, in `smbios/examples/`, outside `libsmbios/src/`.
- `CommandLineParams::format()` - new.
- `main()` - modified to resolve a formatter by name instead of calling
  `render_to_description()` directly.

## Constraints

- `csv_formatter.cpp` must not `#include` any concrete entry-type header
  (`libsmbios/tables/*.hpp`). If it needs to, that is a sign the public
  surface of `get_fields()`/`IOutputFormatter` is insufficient. Treat that
  as a Story 01.0/02.0 gap; do not work around it here.
- No existing `libsmbios` source file and no existing formatter/registry
  file is edited by this task (R5).
- An unknown `--format` value exits non-zero, with a clear, actionable
  message on `stderr`, before any SMBIOS read is attempted.
- The `text` path is byte-identical to the pre-milestone CLI output on
  stdout (R3).

## Tests

- In `tests/integration/CMakeLists.txt`, add plain CTest `add_test`
  entries that invoke the built CLI (`$<TARGET_FILE:smbios>`):
  - `smbios_cli_unknown_format`: runs `--format bogus` with
    `WILL_FAIL TRUE`. It needs no hardware, so it runs in CI.
  - `smbios_cli_unknown_format_lists_names`: the same invocation, with
    `PASS_REGULAR_EXPRESSION` matching `Registered formats:.*csv.*json.*text.*xml.*yaml`.
    It also needs no hardware.
  - `smbios_cli_format_csv`: runs `--format csv`. It reads real hardware,
    so, like the rest of the integration category, it may fail cleanly
    without privileges.

## Success criteria

- [ ] `smbios --format csv` works and produces valid CSV, and
      `csv_formatter.cpp` never includes a concrete entry-type header.
- [ ] `smbios --format bogus` fails fast, with a non-zero exit and the
      registered-names message, before any SMBIOS read is attempted.
- [ ] `smbios` with no `--format`, or with `--format text`, is
      byte-identical on stdout to its pre-milestone output.
- [ ] `smbios --help` lists every registered format name, including `csv`.
