# CLI Usage

The `smbios` executable reads the local machine's SMBIOS/DMI table and prints every structure it can decode.

Not writing C++? `smbios_c` (milestone 0007) is a separate C ABI shim over this project's core library (`libsmbios`), for calling into it from Python, Rust, C#, or any other language with C FFI support, without linking C++ directly - see its header at `smbios_c/include/smbios/smbios.hpp` and [CLAUDE.md](../CLAUDE.md)'s Overview section for the full rationale.

## Running it

```
./smbios
```

No arguments needed for the common case. Typical output:

```
DMI version: 3.4
Table size: 2873
SMBIOS version: 3.4
Table size: 2873
Structures count: 58
Source: macos-uefi
Header ID = 0
BIOS Information:
  Vendor: Apple Inc.
  ...
Header ID = 1
System Information:
  Manufacturer: Apple Inc.
  ...
```

`Source:` reports which `ISmbiosSource` actually supplied the table on this run - e.g. `linux-sysfs`, `macos-uefi`, or `legacy-scan` if every native path was unavailable and the tool fell back to the legacy BIOS physical-memory scan. See [architecture overview](architecture/overview.md) for how that chain is selected.

## Options

| Flag | Long form | Effect |
|------|-----------|--------|
| `-h` | `--help` | Print usage and exit. |
| `-v` | `--version` | Print the version string and exit. |
| `-d <path>` | `--dump-file <path>` | Write the raw SMBIOS table bytes to `<path>` instead of decoding and printing structures. |
| `-f <name>` | `--format <name>` | Output format: `text` (default, the legacy output above, unchanged), `xml`, `json`, `yaml`, or `csv` (an example formatter compiled into the CLI from `smbios/examples/`). An unknown name prints the available formats to stderr and exits 1 before any table is read. |
| `-m` | `--memory-scan` | Parsed but not yet wired - see below. |
| `-r <path>` | `--read-file <path>` | Parsed but not yet wired - see below. |

```
./smbios --dump-file table.bin
```

Two flags are parsed but not yet wired to any behavior - `-m`/`--memory-scan` (intended to force the legacy physical-memory scan, skipping the native source chain) and `-r`/`--read-file` (intended to decode a previously-dumped table instead of reading live hardware). Passing them is accepted but currently has no effect; don't rely on them yet.

## Structured output

```
./smbios --format json
```

Every structure is emitted as one element of a JSON array, with its decoded fields under `fields`. The
same data is available as `xml`, `yaml` and `csv`. See [Output formats](output-formats.md) for each
format's layout and escaping rules, and [Writing a custom output formatter](dev/custom-output-formatter.md)
to add your own.

## Privileges

macOS and Windows need no elevated privileges: macOS reads the `AppleSMBIOS` IORegistry entry via IOKit and Windows uses `GetSystemFirmwareTable()`.

On Linux the first native source reads `/sys/firmware/dmi/tables/smbios_entry_point` and `/sys/firmware/dmi/tables/DMI`. The kernel typically exposes these root-readable only (mode 0400), so an unprivileged run usually cannot open them (the source's availability check is a real open-for-read probe and reports unavailable). The chain then falls through to the UEFI systab source and the legacy memory scan, both of which read `/dev/mem` and also need root. Run `smbios` as root (e.g. with `sudo`) on Linux to get a table; without it the tool reports that no source succeeded (see below). The permission modes are not verified on a CI runner yet, and some distributions may relax them.

## Exit behavior

A failure to read the table at all (no source succeeded) prints an error to stderr and the tool exits non-zero. A structure type this build doesn't yet decode still shows its raw `Header ID`, with no decoded body beneath it - this is expected, not an error; see the structure coverage table in the main [README](../README.md#smbios-structure-coverage) for what's implemented.

## See also

- [Architecture overview](architecture/overview.md) - how a byte on the wire becomes a decoded structure.
- [Test subsystem](test/test-subsystem.md) - what's verified and how, if you're deciding whether to trust this build's output.
- [Fuzzing](dev/fuzzing.md) - the local-only hardening harness for the table parser, if you're auditing untrusted/malformed tables.
