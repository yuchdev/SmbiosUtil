#pragma once
#include <string>

namespace smbios {

struct DMIHeader;

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
};

} // namespace smbios