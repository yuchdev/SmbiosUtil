#pragma once
#include <string>

namespace smbios {

class SMBiosInterface {

public:

    virtual ~SMBiosInterface() = default;

    /// @brief String representation
    virtual std::string get_type() const = 0;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const = 0;

    /// @brief Entry size without string section
    virtual size_t get_entry_size() const = 0;
};

} // namespace smbios