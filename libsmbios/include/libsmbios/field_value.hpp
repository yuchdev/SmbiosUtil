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

/// @brief Stringifies one FieldValue with the encoding every structured
/// formatter shares (milestone 0009 Story 02.0):
/// - `std::string` -> the string as-is, **unescaped**;
/// - `uint64_t` -> its exact decimal digits (e.g. `18446744073709551615`),
///   locale-independent, never via a `double` and never in exponent form;
/// - `bool` -> `"true"` or `"false"`;
/// - `std::vector<uint8_t>` -> hex_encode() of the bytes (two lowercase hex
///   digits per byte, no separators; `""` for an empty vector).
///
/// Only the `std::string` alternative can carry firmware-controlled text;
/// the other three produce only `[0-9a-z]`. The result is therefore *not*
/// safe to embed in any structured format as-is: a formatter must still run
/// a string result through the matching escaper in
/// libsmbios/formatters/formatter_escaping.hpp (e.g. xml_escape()). A
/// formatter that records the alternative (e.g. JSON's bare numbers and
/// booleans) should dispatch on the variant itself rather than use this.
/// Never throws except std::bad_alloc.
/// @param value the field value to stringify.
/// @return the value's textual form, as described above.
std::string field_value_to_string(const FieldValue& value);

} // namespace smbios
