# Story 03.0 - YAML Renderer and Custom-Format Extensibility

**Parent milestone:** [plan.md](../plan.md)
**Status:** 🔶 In progress
**Depends on:** Story 02.0 (registry, built-in formats, shared escaping helpers)

## Scope

This story has four parts:

- Add the hand-rolled YAML renderer.
- Prove that the registry already satisfies the "custom format" ask. One
  concrete, real, non-built-in example does this: a CSV formatter
  registered from outside `libsmbios`'s source tree.
- Wire `--format` into the `smbios` CLI.
- Add runnable examples, generated doc samples, and user and developer
  documentation (R7). This was added 2026-11-10 at the user's request.

## Tasks

| #  | Document                                                                  | Status         | Blocks |
|----|--------------------------------------------------------------------------------|----------------|--------|
| 01 | [YAML renderer](01-yaml-renderer.md)                                           | ✅ Complete (`ff606d3`) | 02     |
| 02 | [Custom-format extensibility example and CLI wiring](02-custom-format-extensibility-example-and-cli-wiring.md) | ✅ Complete (`0d7d781`, `ef62ca0`, `184b692`) | 03     |
| 03 | [Examples and documentation](03-examples-and-documentation.md)                 | ✅ Complete (`3174884`..`c730694`) | 04     |
| 04 | [Tests](04-tests.md)                                                          | ⬜ Not started | -      |

Task 03 depends on Tasks 01 and 02, because its samples need `yaml` and
`csv`, and its CLI smoke test needs `--format`. Task 04 is the exit gate
and depends on Task 03.

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
- Task 03 creates and modifies the following.
  - New examples:
    - `examples/` (new top-level directory): `CMakeLists.txt`,
      `README.md`, `format_table/main.cpp`, `read_fields/main.cpp`,
      `scripts/check_structured_output.py` and `cmake/run_quiet.cmake`.
    - Root `CMakeLists.txt`: add the `SMBIOS_BUILD_EXAMPLES` option and
      `add_subdirectory(examples)`.
  - New doc-sample machinery:
    - `tests/unit/doc_sample_fixture.{hpp,cpp}` and
      `tests/unit/doc_samples_test.cpp`.
    - `tests/data/output_formats/sample.{txt,xml,json,yaml,csv}`.
    - `/.gitattributes`.
    - `scripts/check_doc_samples.py`.
    - `tests/unit/CMakeLists.txt`: add a `SMBIOS_TEST_DATA_DIR`
      definition.
    - `.github/workflows/ci.yml`: extend the ctest regex and add a
      doc-sample check step.
  - New docs: `docs/output-formats.md` and
    `docs/dev/custom-output-formatter.md`. Doxygen comments are added to
    the new public headers.
  - Updated docs: `CLAUDE.md`, the root `README.md`, `docs/cli-usage.md`,
    `docs/architecture/overview.md`, `docs/test/test-subsystem.md` and
    `docs/dev/cpp_coding_standard.md`.
- `docs/roadmap/README.md`: add milestone 0009 (Task 04).
