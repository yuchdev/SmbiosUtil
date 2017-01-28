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

class AbstractSMBiosEntry : public SMBiosInterface {
public:

    AbstractSMBiosEntry(const DMIHeader& header);

    virtual ~AbstractSMBiosEntry() = default;

    size_t get_entry_size() const override;

protected:

    /// Default implementation of SMBIOS string extractor
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

    void parse_dmi_strings();

private:
    
    /// copy of entry header
    std::unique_ptr<DMIHeader> header_;

    std::vector<std::string> dmi_strings_;
};

} // namespace smbios