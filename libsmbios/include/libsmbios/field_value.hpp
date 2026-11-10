#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace smbios {

/// @brief One decoded, renderer-neutral SMBIOS field value (milestone 0009 contract C1).
/// Alternatives:
/// - std::string - decoded text (DMI strings, enumerated-value descriptions);
/// - uint64_t - an integral quantity (sizes, counts, speeds, handles);
/// - bool - a flag, e.g. one set bit of a bitmask property list;
/// - std::vector<uint8_t> - raw bytes (Processor ID, UUID raw form) where
///   stringifying would be lossy or premature; the renderer decides how to
///   represent them, the field model does not.
using FieldValue = std::variant<std::string, uint64_t, bool, std::vector<uint8_t>>;

/// @brief One labelled field of an entry's structured view (milestone 0009 contract C1).
/// Names are unique within one entry's get_fields() result (repeated groups
/// carry a 1-based index suffix), so map-shaped formats never see duplicate keys.
struct NamedField {
    /// @brief Human-readable field label, e.g. "Vendor" or "BIOS Characteristics".
    std::string name;
    /// @brief The decoded value of this field.
    FieldValue value;
};

} // namespace smbios
