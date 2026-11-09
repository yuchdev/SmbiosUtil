#include "synthetic_table_builder.hpp"

#include <stdexcept>

namespace smbios {
namespace test {

namespace {

/// Size, in bytes, of a DMIHeader's on-wire representation: Type (1) +
/// Length (1) + Handle (2) - mirrors smbios/src/smbios.cpp's own
/// kDmiHeaderWireSize, duplicated here since that constant is private to
/// the library translation unit.
constexpr size_t kDmiHeaderWireSize = 4;

} // namespace

SyntheticTableBuilder& SyntheticTableBuilder::add_structure(
    uint8_t type, std::vector<uint8_t> formatted_area, std::vector<std::string> strings)
{
    const size_t length = kDmiHeaderWireSize + formatted_area.size();
    if (length > 0xFF) {
        throw std::invalid_argument(
            "SyntheticTableBuilder::add_structure: formatted_area too large for an 8-bit length field");
    }

    const uint16_t handle = next_handle_++;

    buffer_.push_back(type);
    buffer_.push_back(static_cast<uint8_t>(length));
    buffer_.push_back(static_cast<uint8_t>(handle & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((handle >> 8) & 0xFF));

    buffer_.insert(buffer_.end(), formatted_area.begin(), formatted_area.end());

    if (strings.empty()) {
        // No strings referenced: the string section is just the bare
        // double-NUL marker.
        buffer_.push_back(0x00);
        buffer_.push_back(0x00);
    } else {
        for (const std::string& dmi_string : strings) {
            buffer_.insert(buffer_.end(), dmi_string.begin(), dmi_string.end());
            buffer_.push_back(0x00);
        }
        // One more NUL completes the double-NUL marker (the last string's
        // own terminator is the first of the pair).
        buffer_.push_back(0x00);
    }

    return *this;
}

SyntheticTableBuilder& SyntheticTableBuilder::add_raw_bytes(std::vector<uint8_t> raw)
{
    raw_bytes_added_ = true;
    buffer_.insert(buffer_.end(), raw.begin(), raw.end());
    return *this;
}

std::vector<uint8_t> SyntheticTableBuilder::build() const
{
    std::vector<uint8_t> result = buffer_;

    if (!raw_bytes_added_) {
        // EndOfTable marker: type=127, length=4 (header only, no formatted
        // area/strings) - matches real-world DMI tables and
        // smbios_table_walk_test.cpp's own exact-boundary case.
        result.push_back(static_cast<uint8_t>(SMBios::SMBiosHandler::EndOfTable));
        result.push_back(4);
        result.push_back(0x00);
        result.push_back(0x00);
    }

    return result;
}

} // namespace test
} // namespace smbios
