#include <libsmbios/field_value.hpp>

#include <libsmbios/formatters/formatter_escaping.hpp>

#include <type_traits>

using namespace smbios;

std::string smbios::field_value_to_string(const FieldValue& value)
{
    return std::visit([](const auto& alternative) -> std::string {
        using T = std::decay_t<decltype(alternative)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return alternative; // unescaped: the caller escapes for its format
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::to_string(alternative); // exact decimal digits
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return alternative ? "true" : "false";
        }
        else {
            static_assert(std::is_same_v<T, std::vector<uint8_t>>, "unhandled FieldValue alternative");
            return hex_encode(alternative);
        }
    }, value);
}
