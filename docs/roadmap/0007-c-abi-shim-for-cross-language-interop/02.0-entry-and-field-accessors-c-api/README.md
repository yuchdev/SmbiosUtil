# Story 02.0 - Entry and Field Accessors C API

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (handles, error model, exception boundary)

## Scope

Expose entry iteration, each entry's type/length/handle, every entry's
existing `render_to_description()` string, and a small curated set of
typed getters (per `plan.md`'s Key constraints - not 1:1 coverage of every
C++ getter) - all following Story 01.0's exception-boundary pattern
without exception.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Entry iteration and type dispatch](01-entry-iteration-and-type-dispatch.md) | ⬜ Not started | 02     |
| 02 | [Curated typed field accessors](02-curated-typed-field-accessors.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                | ⬜ Not started | -      |

## Key constraints

- Any function returning a string (`render_to_description()`, a typed
  string getter) follows the standard C "caller-provided buffer" pattern:
  `smbios_error_t smbios_entry_render(smbios_entry_handle_t, char* buf,
  size_t buf_len, size_t* out_needed_len)` - writes up to `buf_len` bytes
  and reports the actual needed length so a caller can retry with a bigger
  buffer, exactly like POSIX `strerror_r`/`readlink` do. No function
  returns a `char*` the caller must separately free with a matching
  `smbios_free_string()` - that doubles the ownership-tracking burden for
  no benefit over the buffer pattern here.
- The curated typed-getter set (Task 02) is **named explicitly** in that
  task's spec, not left open-ended - adding a getter outside that list
  later is a new, separate task in a future milestone, not scope creep
  into this one.

## Files or directories created or modified by this story

- `smbios_c/include/smbios_c/smbios_c.h` - iteration, type/length/handle,
  render, and the curated typed getters.
- `smbios_c/src/smbios_c.cpp` - implementations.
