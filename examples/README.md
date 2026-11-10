# libsmbios examples

Standalone programs that use `libsmbios` the way an external consumer would. They are built by
default; configure with `-DSMBIOS_BUILD_EXAMPLES=OFF` to skip them.

```bash
cmake --preset macos            # or linux / windows; examples are ON by default
cmake --build --preset macos
ctest --test-dir build/macos -R smbios_example_ --output-on-failure
```

## format_table

[`format_table/main.cpp`](format_table/main.cpp) renders the live SMBIOS table through the
output-formatter registry. It shows the three-step pattern: look up a formatter by name before
reading anything, read the table, then frame the document with `begin_document()`, `render()` per
entry, `entry_separator()` between entries and `end_document()`. Run it as
`smbios_example_format_table <format>` with `text`, `xml`, `json` or `yaml`. `csv` is not available
here: it is compiled into the `smbios` CLI only. An unknown or missing name exits with status 2.

## read_fields

[`read_fields/main.cpp`](read_fields/main.cpp) reads typed values from `get_fields()` directly,
without a formatter: the BIOS vendor, version and release date, the number of set BIOS
characteristics, and the locator, size and type of each memory device. Run it as
`smbios_example_read_fields`. It deliberately never prints `Serial Number`, `Asset Tag`,
`Part Number` or `UUID`, because identifying values would end up in CI logs.

## check_structured_output.py

[`scripts/check_structured_output.py`](scripts/check_structured_output.py) runs a command and checks
that it prints one well-formed document in the requested format: JSON and XML are fully parsed, CSV
is parsed with the standard `csv` module (header row plus three-column rows), YAML gets a shape check
only (the standard library has no YAML parser), and `text` only has to be non-empty:

```bash
python3 -I examples/scripts/check_structured_output.py --format json -- smbios --format json
```

It prints only `ok: <format>, <N> structures` (`<N> rows` for CSV) and never echoes any parsed value, so CI logs stay free
of the runner's serial numbers. Python is optional and never needed to build.

## The csv example

The `csv` formatter lives in [`smbios/examples/csv_formatter.cpp`](../smbios/examples/csv_formatter.cpp)
and is compiled into the `smbios` CLI (`smbios --format csv`). The
[custom formatter guide](../docs/dev/custom-output-formatter.md) walks through it. For what each
format emits, see [Output Formats](../docs/output-formats.md).

## Linking note

`examples/CMakeLists.txt` links `libsmbios` as a whole archive, because the built-in formatters and
entry classes register from static initializers. A plain link would make `get_output_formatter()` and
`SMBiosEntryFactory::create()` return `nullptr`.

## When no table can be read

If no SMBIOS table can be read (for example, unprivileged on Linux), `format_table` and
`read_fields` print a notice to stderr and exit 0; `format_table` still writes an empty, well-formed
document. The CTest smoke tests (`smbios_example_*`) therefore assert exit status and well-formedness
only, and discard stdout.
