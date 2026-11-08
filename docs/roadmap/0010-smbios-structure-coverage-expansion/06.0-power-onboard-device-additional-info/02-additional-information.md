# 02 - Additional Information

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `AdditionalInformationEntry` (`SMBiosHandler` value `40`),
DSP0134 3.9.0 section 7.41, through its variable-length Additional
Information Entry array (7.41.1).

## Files

- **Create** `libsmbios/include/smbios/tables/additional_information_entry.h` /
  `.cpp`:
  - 0x04 Number of Additional Information entries `n` (BYTE)
  - 0x05.. Additional Information entries (`n` variable-length entries,
    each shaped per 7.41.1):
    - +0x00 Entry Length (BYTE, minimum 6)
    - +0x01 Referenced Handle (WORD)
    - +0x03 Referenced Offset (BYTE)
    - +0x04 String (BYTE, STRING)
    - +0x05.. Value (variable width = `Entry Length - 5`, raw bytes - see
      this story's own scope-cutoff note)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `AdditionalInformationEntry` - new. `get_entry_count()` (reads the
  explicit count at 0x04 - not derived from `Length`, since each entry is
  itself variable-length and there is no fixed-stride formula like Group
  Associations' or Memory Channel's), and a per-entry accessor walking
  the array by **summing each preceding entry's own `Entry Length`**
  (not a fixed stride) to find entry `i`'s start offset:
  - `get_entry_length_at(size_t index)`
  - `get_referenced_handle_at(size_t index)`
  - `get_referenced_offset_at(size_t index)`
  - `get_string_index_at(size_t index)` + `get_string_at(size_t index)`
    (via `dmi_string()`)
  - `get_value_bytes_at(size_t index)` - returns
    `std::vector<uint8_t>` of length `Entry Length - 5`, per this story's
    own documented scope cutoff (no attempt to interpret the value's
    width/type from the referenced structure).

## Constraints

- Entry walking is inherently O(n) per lookup if each call to
  `get_*_at(index)` re-walks from entry 0 - either accept that cost (this
  structure's `n` is expected to be small - typically a handful of
  enumerated-value overrides, not hundreds) or cache the computed
  per-entry start offsets once in the constructor (a
  `std::vector<size_t> entry_offsets_` populated by one pass over the
  buffer) and have every accessor index into that - the second option is
  recommended given every accessor needs the same walk, but either is
  acceptable; document whichever is chosen in the class's own private
  member comments.
- Every per-entry read must be guarded against `get_entry_size()`
  independently of `get_entry_count()`/each entry's own `Entry Length` -
  a malformed structure whose declared `n` or per-entry `Entry Length`
  values would read past the structure's own bounds must stop cleanly,
  matching this milestone's R2/this codebase's long-standing
  bounds-checking bar (milestone 0003/0006).
- `get_value_bytes_at()`'s raw-bytes return is this task's explicit,
  documented scope cutoff (R8) - do not attempt to special-case common
  referenced-field widths (1/2/4 bytes) by guessing from `Entry Length`
  alone; `Entry Length - 5` already *is* that width, so the raw bytes are
  the complete, correct answer at this structure's own level of
  information - the cutoff is about *interpreting* those bytes as a
  specific C++ type, not about losing any byte.

## Tests

- `tests/unit/additional_information_entry_test.cpp` (new): buffers with
  0, 1, and 3+ entries of varying `Entry Length` (i.e. not all entries
  the same size, to prove the per-entry-length walk is correct and not
  accidentally hardcoded to a fixed stride), plus a malformed buffer
  whose last entry's declared `Entry Length` would overrun
  `get_entry_size()` (verify clean stop, no out-of-bounds read).

## Success criteria

- [ ] Registers and resolves through the factory for `SMBiosHandler ==
      40`.
- [ ] Variable-per-entry-length walk verified with at least one buffer
      containing entries of two different lengths.
- [ ] Malformed-length case does not read past `get_entry_size()`.
- [ ] `smbios` CLI displays the type on real hardware where present.
