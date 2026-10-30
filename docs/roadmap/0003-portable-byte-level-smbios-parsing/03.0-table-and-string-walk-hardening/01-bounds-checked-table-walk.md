# 01 - Bounds-checked table walk

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

`read_smbios_table()`'s loop condition is `current_structure_begin <=
table_end`, which permits dereferencing a 4-byte header starting at
`table_end` itself (reading 3 bytes past the buffer), and
`count_smbios_structures()`'s inner `while` loop dereferences
`*reinterpret_cast<const uint16_t*>(offset)` with only `offset < end_table`
checked **after** the dereference in the loop condition - both at
[smbios.cpp:165,219](/smbios/src/smbios.cpp). Add the missing
pre-dereference checks from contract **C3**.

## Files

- **Modify** `smbios/src/smbios.cpp`, `read_smbios_table()`:
  ```cpp
  for (size_t i = 0; i < number_of_structures; ++i) {
      if (current_structure_begin + sizeof(DMIHeader) > table_end) break; // NEW
      DMIHeader header;
      header.type   = read_u8(current_structure_begin, 0);
      header.length = read_u8(current_structure_begin, 1);
      header.handle = read_u16(current_structure_begin, 2);
      header.data   = current_structure_begin;

      if (header.length < 4) break;
      if (current_structure_begin + header.length > table_end) break; // NEW
      if (header.type == SMBiosHandler::EndOfTable) break;
      headers_list_.push_back(header);

      current_structure_begin += header.length;
      while (current_structure_begin + 1 < table_end &&          // NEW: changed from `<` on a 2-byte read
             (current_structure_begin[0] != 0 || current_structure_begin[1] != 0)) {
          current_structure_begin++;
      }
      current_structure_begin += 2;
  }
  ```
- **Modify** `count_smbios_structures()`: the same `offset + 2 <= end_table`
  style check before any 2-byte read, applied to its own independent copy
  of the same walk.

## Functions and Classes

- `SMBios::read_smbios_table()` - bounds checks added.
- `SMBios::count_smbios_structures()` - bounds checks added.

## Constraints

- Note `read_smbios_table()` and `count_smbios_structures()` duplicate the
  same table-walking logic independently today - this task does not
  deduplicate them into one shared walker (that is a larger refactor out
  of scope here); it applies the same fix to both, accepting the
  duplication as pre-existing.
- `header.length < 4` staying a `break` (not a hard error) matches this
  story's Key constraints - do not change this to a thrown exception.

## Tests

- `tests/unit/smbios_table_walk_test.cpp` (new): a synthetic buffer whose
  last structure's declared `length` would read past the buffer end -
  assert the walk stops cleanly (no crash, no sanitizer report) and
  returns only the structures that were fully in-bounds.

## Success criteria

- [ ] AddressSanitizer (milestone 0002's CI job) reports no out-of-bounds
      read when parsing the new truncated-table fixture.
- [ ] Existing functional test's structure count and content are unchanged
      on real (well-formed) hardware tables.
