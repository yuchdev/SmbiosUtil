# 02 - Memory Device field table

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Delete `MemoryDeviceV21`/`V23`/`V26`/`V27`/`V28` and all five pointer
members ([memory_device_entry.h:20-63,346-350](/smbios/include/smbios/memory_device_entry.h)) -
the deepest version chain in the current codebase - and rebuild every
accessor in `MemoryDeviceEntry` the same way as Task 01.

## Files

- **Modify** `smbios/include/smbios/memory_device_entry.h`: delete all five
  `MemoryDeviceV2x`/`V3x`-equivalent structs and pointer members.
- **Modify** `smbios/src/memory_device_entry.cpp`: rebuild each getter from
  its documented offset, e.g.:
  ```cpp
  uint16_t MemoryDeviceEntry::get_device_speed() const {
      constexpr size_t offset = 0x15;
      if (get_entry_size() < offset + sizeof(uint16_t)) return DeviceSpeedUnknown;
      return read_u16(raw_data(), offset);
  }

  uint8_t MemoryDeviceEntry::get_device_rank() const {
      constexpr size_t offset = 0x1A;
      if (get_entry_size() < offset + sizeof(uint8_t)) return 0;
      return read_u8(raw_data(), offset);
  }
  ```
  across the full offset range (`0x05` array handle through `0x22`
  configured voltage) the five deleted structs collectively covered.

## Functions and Classes

Every public getter keeps its signature. `get_form_factor()` and
`get_device_type()`'s existing "look up in the map, `assert(false)` if
missing" pattern is preserved exactly (R8) - only the source of the raw
byte changes from `memory_device_v21_->device_form_factor` to
`read_u8(raw_data(), 0x0E)`.

## Constraints

- This is the task where the milestone's "five structs become five
  one-line accessors" claim is proven concretely - a reviewer should be
  able to diff this task's before/after and see the struct count for this
  one entry type go from 5 to 0, with no accessor losing any field.
- `get_device_size()`'s existing `& 0x8000` extended-size-marker branch,
  and the separate `extended_size`/`memory_clock_speed` fields
  `MemoryDeviceV27` introduced, must both end up as correctly-offset
  accessors - re-verify both offsets against the DMTF spec during
  conversion (R7), since this is the structure with the most historical
  revisions and the highest chance of an inherited off-by-one.

## Tests

- `tests/unit/memory_device_entry_test.cpp` (new): synthetic buffers at
  each of the five legacy length tiers (`< 0x15`, `0x15`-`0x16`,
  `0x17`-`0x1A`, `0x1B`, `0x1C`-`0x21`, `>= 0x22`), asserting every getter
  returns the correct decoded value or documented default/sentinel at each
  tier.

## Success criteria

- [ ] `MemoryDeviceV21` through `V28` no longer exist anywhere in the
      codebase.
- [ ] Every getter's offset comment matches the DMTF spec.
- [ ] Synthetic-buffer tests pass at every length tier.
- [ ] Existing functional test's Memory Device output is byte-identical
      before/after, on real hardware with populated DIMMs.
