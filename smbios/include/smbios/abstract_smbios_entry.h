#pragma once
#include <string>
#include <map>
#include <sstream>
#include <type_traits>

namespace smbios {

struct DMIHeader;

class SMBiosInterface {

};

class AbstractSMBiosEntry{
public:

    virtual ~AbstractSMBiosEntry() {}

    /// @brief String representation
    virtual std::string get_type() const = 0;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const = 0;

protected:

    /// Default implementation of SMBIOS string extractor
    std::string dmi_string(const DMIHeader& header, size_t string_index);

    /// Default implementation of SMBIOS bitwise properties to string representation
    template <typename T>
    std::string bitset_to_properties(T properties, const std::map<T, std::string>& properties_map) const
    {
        static_assert(std::is_integral<T>(), "Bitwise type should be integer");

        std::stringstream properties_stream;
        for (T current_property = 0x1; current_property; current_property <<= 1) {
            auto it = properties_map.find(current_property);
            if ((properties & current_property) && (it != properties_map.end())) {
                properties_stream << '\t' << (*it).second << '\n';
            }
        }
        return std::move(properties_stream.str());
    }
};

} // namespace smbios