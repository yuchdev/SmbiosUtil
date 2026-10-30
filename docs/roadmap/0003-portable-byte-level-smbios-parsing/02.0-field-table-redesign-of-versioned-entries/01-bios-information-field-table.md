# 01 - BIOS Information field table

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Delete `BiosInformationV24`/`BiosInformationV31` and the
`bios_information24_`/`bios_information31_` pointer members
([bios_information_entry.h:19-41,236-237](/smbios/include/smbios/bios_information_entry.h)),
and rebuild every accessor in `BiosInformationEntry` to read directly from
`raw_data()` via `read_uN` plus a length guard.

## Files

- **Modify** `smbios/include/smbios/abstract_smbios_entry.h` /
  `.cpp`: add
  ```cpp
  protected:
      const uint8_t* raw_data() const { return header_->data; }
  ```
- **Modify** `smbios/include/smbios/bios_information_entry.h`: delete the
  `BiosInformationV24`/`V31` structs and their `#pragma pack` wrapper;
  delete `bios_information24_`/`bios_information31_` members.
- **Modify** `smbios/src/bios_information_entry.cpp`: rebuild each getter,
  e.g.:
  ```cpp
  uint8_t BiosInformationEntry::get_vendor_index() const {
      constexpr size_t offset = 0x04;
      if (get_entry_size() < offset + sizeof(uint8_t)) return 0;
      return read_u8(raw_data(), offset);
  }

  uint32_t BiosInformationEntry::get_runtime_size() const {
      constexpr size_t offset = 0x06; // starting_segment, same offset as get_starting_address
      if (get_entry_size() < offset + sizeof(uint16_t)) return 0;
      const uint16_t starting_segment = read_u16(raw_data(), offset);
      return static_cast<uint32_t>((0x10000 - starting_segment) << 4); // unchanged formula
  }

  uint64_t BiosInformationEntry::get_properties() const {
      constexpr size_t offset = 0x0A;
      if (get_entry_size() < offset + sizeof(uint64_t)) return 0;
      return read_u64(raw_data(), offset);
  }
  ```
  and so on for every offset already documented in the header's `@brief
  0xNN offset` comments (`0x04` vendor, `0x05` version, `0x06` starting
  segment, `0x08` release date, `0x09` ROM size, `0x0A` properties, `0x12`/
  `0x13` extension bytes 1/2, `0x14`-`0x17` BIOS/firmware major/minor, and
  `0x18` extended ROM size once `header.length > 0x18` - the field the
  deleted `BiosInformationV31` struct alone carried).
- The constructor's version/length gating
  (`if ((header.length < 0x12) || (version < SMBiosVersion{2, 1})) return;`)
  is replaced by per-field length checks inside each accessor (R2) - the
  constructor no longer needs to branch on version at all for this entry
  type, since every field BiosInformationV24/V31 distinguished is
  distinguished by `length` alone, not by a version-only signal. Confirm
  this during implementation; if a genuine version-only field exists,
  gate it with the corrected `SMBiosVersion` comparison from milestone 0002
  (R3), not by reintroducing a pointer member.

## Functions and Classes

Every public getter on `BiosInformationEntry` keeps its exact signature;
only the body changes. `init_string_values()` and the bitmask-to-string
maps (`properties_map_`, etc.) are untouched - they operate on the `uint64_t`/
`uint8_t` value already decoded, not on raw bytes.

## Constraints

- `get_rom_size()`'s existing `0xFF` sentinel check and
  `get_runtime_size_string()`'s `& 0x000003FF` branch are **behavior**
  (R8) - preserve exactly.
- Extended ROM size (`0x18` offset, `BiosInformationV31` in the old code)
  needs a new accessor (`get_extended_rom_size()` or similar) if the
  current header exposes it only implicitly through the deleted struct -
  check whether a public getter already exists; if not, this task may need
  to add one to avoid silently dropping functionality the old (if unused)
  `bios_information31_` member exposed.

## Tests

- `tests/unit/bios_information_entry_test.cpp` (new, or extended
  functional test): construct a `BiosInformationEntry` against a synthetic
  byte buffer at each of the three length tiers (`< 0x12`, `0x12`-`0x18`,
  `> 0x18`) and assert each getter returns the expected decoded value or
  default.

## Success criteria

- [ ] `BiosInformationV24`/`V31` no longer exist anywhere in the codebase.
- [ ] Every getter's offset comment matches the DMTF spec (re-verified,
      not just copied forward).
- [ ] Synthetic-buffer tests pass at all three length tiers.
- [ ] Existing functional test's BIOS Information output is byte-identical
      before/after, on real hardware.
