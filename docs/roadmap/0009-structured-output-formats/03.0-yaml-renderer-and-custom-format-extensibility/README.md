# Story 03.0 - YAML Renderer and Custom-Format Extensibility

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (registry and built-in formats)

## Scope

Add the YAML renderer, prove the "custom format" ask is already satisfied
by the registry via one concrete, real, non-built-in example (a CSV
formatter registered from outside `smbios`'s own source tree), and wire
`--format` into `smbios_util`'s CLI.

## Tasks

| #  | Document                                                                  | Status         | Blocks |
|----|--------------------------------------------------------------------------------|----------------|--------|
| 01 | [YAML renderer](01-yaml-renderer.md)                                           | ⬜ Not started | 02     |
| 02 | [Custom-format extensibility example and CLI wiring](02-custom-format-extensibility-example-and-cli-wiring.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                          | ⬜ Not started | -      |

## Key constraints

- The CSV example formatter (Task 02) is **not** registered inside
  `smbios`'s own `src/` - it lives under `smbios_util/examples/` (or
  equivalent), proving a formatter can be added without touching the
  library itself, which is the actual point R5 is making.
- `--format`'s validation (R6) happens before `SMBios bios;` is
  constructed in `main.cpp` - fail fast on an unknown format name, don't
  waste a real SMBIOS read (which may require elevated privileges, per
  milestone 0005) only to then reject the output format.

## Files or directories created or modified by this story

- `smbios/include/smbios/yaml_formatter.h` / `.cpp` (new).
- `smbios_util/examples/csv_formatter.cpp` (new) - the custom-format
  proof.
- `smbios_util/include/smbios_util/command_line_parser.h` /
  `smbios_util/src/command_line_parser.cpp` / `main.cpp` - `--format`
  option.
