# 01 - Byte reader utility

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C1**: four free functions that decode an unsigned
integer of known width from a raw byte pointer at a given offset, composing
the result byte-by-byte so the host's own endianness never participates in
the computation (SMBIOS wire format is little-endian per the DMTF spec,
regardless of host).

## Files

- **Create** `smbios/include/smbios/byte_reader.h`:
  ```cpp
  #pragma once
  #include <cstdint>
  #include <cstddef>

  namespace smbios {

  /// @brief Read one little-endian byte (trivial; provided for symmetry).
  constexpr uint8_t read_u8(const uint8_t* data, size_t offset) {
      return data[offset];
  }

  /// @brief Read a little-endian uint16_t, independent of host endianness.
  constexpr uint16_t read_u16(const uint8_t* data, size_t offset) {
      return static_cast<uint16_t>(data[offset]) |
             (static_cast<uint16_t>(data[offset + 1]) << 8);
  }

  /// @brief Read a little-endian uint32_t, independent of host endianness.
  constexpr uint32_t read_u32(const uint8_t* data, size_t offset) {
      return static_cast<uint32_t>(data[offset]) |
             (static_cast<uint32_t>(data[offset + 1]) << 8) |
             (static_cast<uint32_t>(data[offset + 2]) << 16) |
             (static_cast<uint32_t>(data[offset + 3]) << 24);
  }

  /// @brief Read a little-endian uint64_t, independent of host endianness.
  constexpr uint64_t read_u64(const uint8_t* data, size_t offset) {
      return static_cast<uint64_t>(read_u32(data, offset)) |
             (static_cast<uint64_t>(read_u32(data, offset + 4)) << 32);
  }

  } // namespace smbios
  ```

## Functions and Classes

- `smbios::read_u8/u16/u32/u64(const uint8_t*, size_t)` - `constexpr`,
  header-only, no bounds checking (caller's responsibility per this
  story's Key constraints).

## Constraints

- No `reinterpret_cast`, no `memcpy`, no dependency on `std::endian` or
  `#pragma pack` - the byte composition itself is what makes this
  endian-independent, not a runtime/compile-time endianness branch.
- `constexpr` so a future compile-time unit test (`static_assert`) can
  verify correctness without running anything.

## Tests

- `tests/unit/byte_reader_test.cpp` (new): decode a known byte sequence
  (e.g. `{0x34, 0x12}` → `read_u16` → `0x1234`) for each width; a
  `static_assert` using the `constexpr` property for at least one case.

## Success criteria

- [ ] `read_u16({0x34, 0x12}, 0) == 0x1234` (classic endian-swap sanity
      check - wrong on a naive `reinterpret_cast` on a hypothetical
      big-endian host, right here on every host).
- [ ] All four functions have at least one `static_assert`-verified case.
