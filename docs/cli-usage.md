# CLI Usage

The `smbios` executable reads the local machine's SMBIOS/DMI table and prints every structure it can decode.

Not writing C++? `smbios_c` (milestone 0007) is a separate C ABI shim over this project's core library (`libsmbios`), for calling into it from Python, Rust, C#, or any other language with C FFI support, without linking C++ directly - see its header at `smbios_c/include/smbios_c/smbios_c.h` and [CLAUDE.md](../CLAUDE.md)'s Overview section for the full rationale.

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

```
./smbios --dump-file table.bin
```

Two flags are parsed but not yet wired to any behavior - `-m`/`--memory-scan` (intended to force the legacy physical-memory scan, skipping the native source chain) and `-r`/`--read-file` (intended to decode a previously-dumped table instead of reading live hardware). Passing them is accepted but currently has no effect; don't rely on them yet.

## Privileges

No elevated privileges are required on any platform. Linux reads `/sys/firmware/dmi/tables` (then the UEFI systab) and macOS reads the `AppleSMBIOS` IORegistry entry via IOKit - both are plain, unprivileged reads. Windows uses `GetSystemFirmwareTable()`. All three fall back to the legacy BIOS memory scan only if every native path is unavailable; that fallback *may* need elevated privileges depending on the platform's own memory-access rules.

## Exit behavior

A failure to read the table at all (no source succeeded) prints an error to stderr and the tool exits non-zero. A structure type this build doesn't yet decode still shows its raw `Header ID`, with no decoded body beneath it - this is expected, not an error; see the structure coverage table in the main [README](../README.md#smbios-structure-coverage) for what's implemented.

## See also

- [Architecture overview](architecture/overview.md) - how a byte on the wire becomes a decoded structure.
- [Test subsystem](test/test-subsystem.md) - what's verified and how, if you're deciding whether to trust this build's output.
- [Fuzzing](dev/fuzzing.md) - the local-only hardening harness for the table parser, if you're auditing untrusted/malformed tables.
