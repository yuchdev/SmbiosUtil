# libsmbios examples

Standalone programs that use `libsmbios` the way an external consumer
would. They are built by default; configure with
`-DSMBIOS_BUILD_EXAMPLES=OFF` to skip them.

```
cmake --preset macos            # or linux / windows; examples are ON by default
cmake --build --preset macos
ctest --test-dir build/macos -R smbios_example_ --output-on-failure
```

- `format_table/main.cpp` (`smbios_example_format_table <format>`) renders
  the live SMBIOS table as `text`, `xml`, `json` or `yaml` through the
  output-formatter registry.
- `read_fields/main.cpp` (`smbios_example_read_fields`) reads typed field
  values from `get_fields()` directly, without a formatter.
- `scripts/check_structured_output.py` (`python3 -I check_structured_output.py --format json -- <command>`)
  checks that a command prints one well-formed JSON or XML document.
- The `csv` formatter example lives in
  [`smbios/examples/csv_formatter.cpp`](../smbios/examples/csv_formatter.cpp)
  and is compiled into the `smbios` CLI (`smbios --format csv`).

If no SMBIOS table can be read (for example, unprivileged on Linux), the
examples print a notice to stderr, write an empty document, and exit 0.
