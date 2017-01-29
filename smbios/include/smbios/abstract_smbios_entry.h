#pragma once
#include <map>
#include <sstream>
#include <type_traits>
#include <vector>
#include <string>
#include <memory>
#include <smbios/smbios_entry_interface.h>

namespace smbios {

struct DMIHeader;

/// @brief Basic functionality implementation for any SMBIOS entry
/// Working with DMI strings, offsets, convert bitwise properties to description etc
/// Class instance does not "own" this memory, it just provide more convenient interface
/// and useful types and enumerations
class AbstractSMBiosEntry : public SMBiosInterface {
public:

    /// @brief Accept a copy of own header
    AbstractSMBiosEntry(const DMIHeader& header);

    // @brief Parent is abstract
    virtual ~AbstractSMBiosEntry() = default;

    /// @brief Entry size without string section
    size_t get_entry_size() const override;

protected:

    /// Implementation of SMBIOS string extractor
    /// Note: First string index is 1, 0 is "Not Specified"
    std::string dmi_string(size_t string_index) const;

    /// Print segment-based offset
    std::string address_string(uint16_t string_index) const;

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
private:

    /// Extract all strings from the string section
    /// which is plain char array separated with \0 symbols
    /// End of section is \0\0 sequence, which it followed by the next SMBIOS entry
    /// There is no limit on the length of each individual text string
    void parse_dmi_strings();

private:
    
    /// copy of entry header
    std::unique_ptr<DMIHeader> header_;

    /// Extracted DMI strings
    std::vector<std::string> dmi_strings_;
};

} // namespace smbios