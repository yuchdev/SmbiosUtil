# Story 02.0 - Renderer Registry and Built-in Formats

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (`get_fields()` on every entry type)

## Scope

Implement `IOutputFormatter` and its self-registering registry (contract
**C2**, mirroring milestone 0004 Story 01.0's entry-factory pattern), and
the three built-in formats `text`/`xml`/`json`.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [Output formatter registry](01-output-formatter-registry.md)           | ⬜ Not started | 02     |
| 02 | [Text, XML and JSON renderers](02-text-xml-and-json-renderers.md)       | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                   | ⬜ Not started | -      |

## Key constraints

- The registry's shape **deliberately mirrors**
  `smbios_entry_factory.h`/`.cpp` (post milestone 0004) - same
  self-registration macro idiom, same "look up by string key, return
  `nullptr`/throw on miss" contract - so a reader who understands one
  already understands the other.
- `text` is the **only** formatter permitted to bypass `get_fields()` and
  call `render_to_description()` directly (R4's stated exception) - for
  exact backward compatibility with every existing `smbios_util`
  invocation.
- The JSON library choice (Boost.JSON vs. nlohmann vs. Boost.PropertyTree
  - see `status.md`) is made and recorded at the start of Task 02, before
  writing the JSON renderer, not discovered partway through.

## Files or directories created or modified by this story

- `smbios/include/smbios/output_formatter.h` / `.cpp` (new).
- `smbios/include/smbios/text_formatter.h` / `.cpp`,
  `xml_formatter.h`/`.cpp`, `json_formatter.h`/`.cpp` (new).
- `vcpkg.json` (milestone 0001) - a new JSON library dependency, only if
  Task 02's decision isn't Boost.JSON (already present transitively).
