# Story 01.0 - Structured Field Model

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Milestone 0004 (all eleven entry types must exist)

## Scope

Define `FieldValue`/`NamedField` (contract **C1**), add the pure virtual
`get_fields()` to `AbstractSMBiosEntry`, and implement it on every one of
milestone 0004's eleven entry types - the foundation every renderer in
Story 02.0/03.0 builds on.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------------|----------------|--------|
| 01 | [FieldValue and entry interface](01-fieldvalue-and-entry-interface.md)   | ⬜ Not started | 02     |
| 02 | [Migrate entries to structured fields](02-migrate-entries-to-structured-fields.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                   | ⬜ Not started | -      |

## Key constraints

- `get_fields()` is **pure virtual** - every entry type must implement
  it; there is no default/fallback implementation that would let a future
  twelfth entry type silently ship without structured-output support.
- `get_fields()` must not re-derive any value independently of the
  existing typed getters (R1) - it calls `get_vendor_string()`,
  `get_properties()`, etc., exactly as `render_to_description()` already
  does, just packaging the results as `NamedField`s instead of
  stream-inserting them into prose.
- A bitmask property becomes one `NamedField` per set bit, not one
  comma-joined string field (per `plan.md`'s C1 note) - this is the one
  place this story changes *shape*, not just *packaging*, versus the
  existing `render_to_description()` text.

## Files or directories created or modified by this story

- `smbios/include/smbios/field_value.h` (new).
- `smbios/include/smbios/abstract_smbios_entry.hpp` - new pure virtual
  `get_fields()`.
- Every one of the eleven entry headers/sources from milestone 0004 -
  each gains a `get_fields()` implementation.
