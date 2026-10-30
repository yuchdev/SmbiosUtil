# 03 - Port Connection field table

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Convert `PortConnectionEntry`'s single `PortConnection` struct and
`port_connection_` member ([port_connection_entry.h:20-27,175](/smbios/include/smbios/port_connection_entry.h))
to the same accessor pattern, for consistency (R5) - this entry has no
version chain to collapse (confirmed: "fully compatible with all previous
versions" per its own header comment), so this is the simplest of the
three conversions in this story.

## Files

- **Modify** `smbios/include/smbios/port_connection_entry.h`: delete the
  `PortConnection` struct and its `#pragma pack` wrapper; delete
  `port_connection_` member.
- **Modify** `smbios/src/port_connection_entry.cpp`: rebuild the three
  getters:
  ```cpp
  uint8_t PortConnectionEntry::get_internal_connection_type() const {
      constexpr size_t offset = 0x05;
      if (get_entry_size() < offset + sizeof(uint8_t)) return 0;
      return read_u8(raw_data(), offset);
  }
  // get_external_connection_type() at 0x07, get_port_type() at 0x08, same pattern
  ```

## Functions and Classes

`get_internal_connection_type()`, `get_external_connection_type()`,
`get_port_type()` keep their signatures; bodies rebuilt as above.

## Constraints

- Since there is no version gating to preserve for this entry, the
  constructor's role shrinks to the existing type-check
  (`if (header.type != SMBios::PortConnection) throw ...`) only - do not
  invent a length guard in the constructor where each accessor already
  guards itself (R2 applies per-field, not just at construction).

## Tests

- `tests/unit/port_connection_entry_test.cpp` (new): synthetic buffer at
  minimum valid length, asserting all three getters decode correctly.

## Success criteria

- [ ] `PortConnection` struct no longer exists.
- [ ] Existing functional test's Port Connection output is byte-identical
      before/after, on real hardware with at least one port connector
      present.
