# Writing a Custom Output Formatter

This guide shows how to add an output format (the `csv` format is the worked example) without editing
any `libsmbios` source. For what the built-in formats emit, see [Output Formats](../output-formats.md).

## When to write a formatter

Write an `IOutputFormatter` when you want `smbios --format <name>` (or your own program through
`get_output_formatter()`) to produce a whole document in a new syntax. If you only need a few values
inside your own program, skip formatters and read `AbstractSMBiosEntry::get_fields()` directly; see
[`examples/read_fields/main.cpp`](../../examples/read_fields/main.cpp).

## Walkthrough: the CSV formatter

The complete source is [`smbios/examples/csv_formatter.cpp`](../../smbios/examples/csv_formatter.cpp).
Line numbers below refer to it.

1. **Include only the public headers (lines 51-53).** `<libsmbios/output_formatter.hpp>`,
   `<libsmbios/abstract_smbios_entry.hpp>` and `<libsmbios/formatters/formatter_escaping.hpp>`. Never
   include `libsmbios/tables/*.hpp`: a formatter sees every entry only through `get_type()` and
   `get_fields()`.
2. **Subclass `IOutputFormatter` (lines 121-143).** Implement `render(const AbstractSMBiosEntry&)`,
   which returns the text for one entry with no document framing, and `format_name()`, the
   lowercase, case-sensitive registry key (`"csv"`).
3. **Optionally add framing (line 147).** `begin_document()`, `entry_separator()` and
   `end_document()` default to empty strings. CSV overrides only `begin_document()` for its header
   row; JSON uses all three. The caller builds a document as `begin_document()`, each `render()`
   with `entry_separator()` between consecutive entries, then `end_document()`;
   `render_document()` in `output_formatter.hpp` does exactly that. The hooks take no entry, so they
   can never carry firmware data. The document must be well formed for zero, one and many entries.
4. **Visit each `FieldValue` (lines 94-106).** A field is a `std::string`, `uint64_t`, `bool` or
   `std::vector<uint8_t>`. CSV uses `std::get_if`; `std::visit` works equally well.
   `field_value_to_string()` (`field_value.hpp`) is available for formats that carry no type
   information.
5. **Reuse the escaping helpers (lines 66-89, 105).** `sanitize_utf8()` makes every string valid
   UTF-8, and `hex_encode()` renders bytes. `xml_escape()`, `json_escape()` and
   `yaml_double_quoted_escape()` exist for those syntaxes. Never hand-roll a UTF-8 decoder: the
   helpers are strict and shared, and invalid input can then never combine with an escape to form a
   structural character. CSV adds its own format-specific rules on top (RFC 4180 quoting and the
   formula-injection guard).
6. **Register in the `.cpp` only (line 152).** `REGISTER_OUTPUT_FORMATTER(smbios_examples::CsvFormatter)`
   at namespace scope. Never put it in a header: it would register once per translation unit that
   includes the header and trip the duplicate-name check (the same trap as `REGISTER_SMBIOS_ENTRY`).

## Registration and linking pitfalls

Registration happens in a static initializer, so it only runs if the formatter's object file is
linked into the binary.

- **Static archives drop unreferenced objects.** `libsmbios.a` holds the built-in formatters and
  every entry class; nothing names them directly, so link it as a whole archive (`-force_load` on
  Apple, `/WHOLEARCHIVE` on MSVC, `--whole-archive` elsewhere). Without it,
  `get_output_formatter("json")` returns `nullptr`. Demonstrated in
  [`smbios/src/CMakeLists.txt`](../../smbios/src/CMakeLists.txt),
  [`examples/CMakeLists.txt`](../../examples/CMakeLists.txt),
  [`smbios_c/CMakeLists.txt`](../../smbios_c/CMakeLists.txt) and
  [`tests/c_abi/CMakeLists.txt`](../../tests/c_abi/CMakeLists.txt).
- **Compile your formatter straight into the executable.** Add its object file to the executable's
  sources instead of a static archive and no linker flag is needed. This is what
  [`smbios/examples/CMakeLists.txt`](../../smbios/examples/CMakeLists.txt) does for `csv`
  (an OBJECT library added through `target_sources`).
- **`smbios_unit_test` does not force-load.** Define the members out of line and name the class in
  the test so its object is linked; see [`tests/unit/CMakeLists.txt`](../../tests/unit/CMakeLists.txt),
  which compiles `csv_formatter.cpp` directly into the test.
- **Registration is static-init only and not thread-safe.** Do not register after `main()` starts.
- **Bad names fail fast.** An empty name or null formatter throws `std::invalid_argument`; a duplicate
  name throws `std::logic_error`. From the macro, that happens in a static initializer, so the
  process terminates at load time.

## Rules a formatter must keep

- Render exactly what `get_fields()` returns, in order: no clamping, dropping or synthesising.
- Field names and types are values, never element or attribute names (CWE-91).
- Treat every string as untrusted and escape it with the shared helpers.
- Never log or persist field values.
- Be deterministic: no timestamps, host or locale data, and `\n` line endings.
- Never throw from `render()` or the hooks, whatever bytes the strings hold.

## Testing a formatter

- Use `FieldsTestEntry` and the hostile-string corpus in
  [`tests/unit/formatter_test_support.hpp`](../../tests/unit/formatter_test_support.hpp) for exact
  values, and `SyntheticTableBuilder` for real entries.
- Use the structural validators in
  [`tests/unit/structured_output_validators.hpp`](../../tests/unit/structured_output_validators.hpp)
  (`is_well_formed_json`, `is_well_formed_xml`, `is_well_formed_flat_yaml`).
- See [`tests/unit/csv_formatter_test.cpp`](../../tests/unit/csv_formatter_test.cpp) for a complete
  example.
- A built-in format also gets a golden sample in `tests/unit/doc_samples_test.cpp`.

## Regenerating doc samples

The samples in [Output Formats](../output-formats.md) are copies of
`tests/data/output_formats/sample.*`. After a deliberate format change:

```bash
# run from the directory holding smbios_unit_test
SMBIOS_REGENERATE_DOC_SAMPLES=1 ./smbios_unit_test --run_test=DocSamplesTests
python3 scripts/check_doc_samples.py --write
python3 scripts/check_doc_samples.py --check
```

Review the diff of the golden files before committing.
