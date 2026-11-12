# 4. Process: `smbios` CLI run

Control flow of `smbios/src/main.cpp`. Mermaid, not rendered locally.

```mermaid
flowchart TD
    A["main: parse args<br/>CommandLineParams::read_params"] --> B{"parse error?"}
    B -->|yes| B1["print error and options<br/>usage exits 0"]
    B -->|no| C{"--help or --version?"}
    C -->|yes| C1["print and exit 0"]
    C -->|no| D["get_output_formatter for --format<br/>default text"]
    D --> E{"formatter found?"}
    E -->|no| E1["stderr: unknown format and<br/>registered names, exit 1<br/>before any table read"]
    E -->|yes| F["SMBios bios<br/>see diagram 3"]
    F --> G{"text format<br/>or --dump-file?"}
    G -->|yes| H["print DMI version, table size,<br/>render_to_description"]
    G -->|no| K
    H --> I{"--dump-file given?"}
    I -->|yes| I1["write raw table to file, exit 0"]
    I -->|no| J["per DMIHeader: print Header ID,<br/>SMBiosEntryFactory::create,<br/>formatter render if not nullptr"]
    K["create entry per DMIHeader<br/>nullptr kept"] --> L["render_document<br/>begin, render, separator, end"]
    J --> M["stdout"]
    L --> M
```

Notes and discrepancies for the reviewer:

- Only the structured formats (`xml`, `json`, `yaml`, `csv`) go through
  `render_document()`. The `text` format (and `--dump-file`) keeps the
  legacy per-header loop that calls `formatter->render()` directly, to stay
  byte-identical to the pre-formatter output.
- `--dump-file` ignores `--format`.
- `--read-file` and `--memory-scan` are accepted by the parser
  (`smbios/src/command_line_parser.cpp`) but `main.cpp` does not act on
  them: the read-file branch is an empty `TODO`, and `--memory-scan` is
  never read after parsing. They are left out of the diagram on purpose;
  this looks like unfinished work, not a diagram error.
- An exception while reading or rendering is reported on stderr as
  "Unable to read SMBIOS table" and `main` still returns `0`.
