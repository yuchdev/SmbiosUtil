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
