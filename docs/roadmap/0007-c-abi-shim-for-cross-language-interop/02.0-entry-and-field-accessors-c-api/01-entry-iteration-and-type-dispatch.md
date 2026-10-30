# 01 - Entry iteration and type dispatch

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Expose `SMBios`'s existing C++ iterator (`begin()`/`end()` over
`DMIHeader`s) as a C-callable "get entry count, get entry by index"
pair - simpler for a C/FFI caller than mirroring a C++ iterator - plus
each entry's `type`/`length`/`handle` and its rendered description
string.

## Files

- **Modify** `smbios_c/include/smbios_c/smbios_c.h`:
  ```c
  smbios_error_t smbios_entry_count(smbios_handle_t, size_t* out_count);
  smbios_error_t smbios_entry_at(smbios_handle_t, size_t index, smbios_entry_handle_t* out_entry);
  /* smbios_entry_handle_t is a non-owning view into data owned by smbios_handle_t -
     valid only until smbios_close() is called on the parent handle; never
     freed separately. */

  uint8_t smbios_entry_type(smbios_entry_handle_t);
  uint8_t smbios_entry_length(smbios_entry_handle_t);
  uint16_t smbios_entry_handle_id(smbios_entry_handle_t); /* DMIHeader::handle, not the opaque smbios_entry_handle_t itself */
  smbios_error_t smbios_entry_render(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  ```
- **Modify** `smbios_c/src/smbios_c.cpp`: implementations backed by
  `SMBios::get_structures_count()`/iterator and
  `SMBiosEntryFactory::create()` (milestone 0004) to get a concrete
  `AbstractSMBiosEntry` for each `DMIHeader`, whose
  `render_to_description()` backs `smbios_entry_render()`.

## Functions and Classes

- `smbios_entry_count`, `smbios_entry_at`, `smbios_entry_type`,
  `smbios_entry_length`, `smbios_entry_handle_id`, `smbios_entry_render` -
  new.

## Constraints

- `smbios_entry_handle_t`'s lifetime is tied to its parent
  `smbios_handle_t` (documented in the header comment above) - this is a
  deliberate simplification over a fully independent, separately-closable
  handle, since every current `AbstractSMBiosEntry` is cheap to construct
  per access rather than cached; revisit only if profiling shows repeated
  `smbios_entry_at()` calls are a bottleneck for a real caller.
- `smbios_entry_render()` must handle the case where
  `SMBiosEntryFactory::create()` returns `nullptr` (an unregistered type -
  should not happen after milestone 0004 covers all eleven
  `SMBiosHandler` values, but the factory's contract still allows it) by
  writing a short placeholder string ("Unknown structure type N") rather
  than returning an error - a caller iterating every entry should not
  have to special-case one unregistered type among many known ones.

## Tests

- `tests/c_abi/smbios_entry_iteration_test.cpp` (new): on real hardware,
  `smbios_entry_count()` matches `SMBios::get_structures_count()`;
  iterating all entries and calling `smbios_entry_render()` on each
  produces non-empty strings matching the equivalent C++ calls'
  `render_to_description()` output byte-for-byte.

## Success criteria

- [ ] Entry count and iteration match the C++ API exactly on real
      hardware.
- [ ] `smbios_entry_render()`'s buffer-too-small path (`buf_len` smaller
      than the real string) reports `SMBIOS_ERR_BUFFER_TOO_SMALL` and the
      correct `out_needed_len`, verified by test.
