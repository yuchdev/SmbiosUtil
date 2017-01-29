#pragma once
#include <string>

namespace smbios {

/// @brief Interface for any SMBIOS entry
class SMBiosInterface {

public:

    /// @brief Hierarchy is polymorph
    virtual ~SMBiosInterface() = default;

    /// @brief Entry string representation
    virtual std::string get_type() const = 0;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const = 0;

    /// @brief Entry size without string section
    virtual size_t get_entry_size() const = 0;
};

} // namespace smbios