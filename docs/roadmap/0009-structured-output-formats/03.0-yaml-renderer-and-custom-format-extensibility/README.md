# Story 03.0 - YAML Renderer and Custom-Format Extensibility

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (registry, built-in formats, shared escaping helpers)

## Scope

This story has three parts:

- Add the hand-rolled YAML renderer.
- Prove that the registry already satisfies the "custom format" ask. One
  concrete, real, non-built-in example does this: a CSV formatter
  registered from outside `libsmbios`'s source tree.
- Wire `--format` into the `smbios` CLI.

## Tasks

| #  | Document                                                                  | Status         | Blocks |
|----|--------------------------------------------------------------------------------|----------------|--------|
| 01 | [YAML renderer](01-yaml-renderer.md)                                           | ⬜ Not started | 02     |
| 02 | [Custom-format extensibility example and CLI wiring](02-custom-format-extensibility-example-and-cli-wiring.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                          | ⬜ Not started | -      |

## Key constraints

- **The CSV formatter lives outside `libsmbios`.** The CSV example
  formatter (Task 02) is **not** registered inside `libsmbios/src/`. It
  lives in `smbios/examples/` and is compiled straight into the `smbios`
  executable as an object, not through a static archive, so its
  `REGISTER_OUTPUT_FORMATTER` runs without any whole-archive flag. This
  proves a formatter can be added without touching the library, which is
  the actual point R5 is making.
- **R5's "zero changes"** means zero edits to existing `libsmbios` and
  `smbios` *source* files, and no edit to any formatter or registry file.
  The single permitted touch to an existing file is build wiring: one
  `add_subdirectory(examples)` line in `smbios/CMakeLists.txt`. That line
  is required because `smbios/src/CMakeLists.txt` globs only
  `smbios/src/*.cpp`. R6's `--format` edits to `main.cpp` and
  `command_line_parser.*` are their own requirement, and they are
  format-agnostic (none of them names CSV).
- **Validate `--format` before reading SMBIOS.** `--format` validation
  (R6) happens before `SMBios bios;` is constructed in
  `smbios/src/main.cpp`. Fail fast on an unknown format name. Do not spend
  a real SMBIOS read, which may need elevated privileges on the
  legacy-scan path, only to then reject the output format.
- **YAML is hand-rolled** (ratified 2026-11-10). The emitter is flat, and
  every string is double-quoted. There is no `yaml-cpp`.

## Files or directories created or modified by this story

- `libsmbios/include/libsmbios/formatters/yaml_formatter.hpp` and
  `libsmbios/src/yaml_formatter.cpp` (new).
- `smbios/examples/csv_formatter.cpp` and `smbios/examples/CMakeLists.txt`
  (new). The CSV example proves the custom-format extension point.
- `smbios/CMakeLists.txt`: add `add_subdirectory(examples)`.
- `smbios/include/smbios/command_line_parser.hpp`,
  `smbios/src/command_line_parser.cpp` and `smbios/src/main.cpp`: add the
  `--format` option.
- `tests/integration/CMakeLists.txt`: add CLI smoke tests (Task 02).
- `CLAUDE.md`, the root `README.md`, `docs/cli-usage.md` and
  `docs/roadmap/README.md`: documentation updates (Task 03).
