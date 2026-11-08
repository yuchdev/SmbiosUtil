#include <smbios/tables/system_configuration_options_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::SystemConfigurationOptions at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemConfigurationOptions, SystemConfigurationOptionsEntry)

SystemConfigurationOptionsEntry::SystemConfigurationOptionsEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemConfigurationOptions) {
        std::stringstream err;
        err << "Wrong entry type, expected System Configuration Options, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }
}

std::string SystemConfigurationOptionsEntry::get_type() const
{
    return "System Configuration Options";
}

std::string SystemConfigurationOptionsEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    const uint8_t count = get_option_count();
    description << "Option Count: " << static_cast<unsigned>(count) << '\n';
    for (uint8_t index = 1; index <= count; ++index) {
        description << "Option " << static_cast<unsigned>(index) << ": " << get_option_at(index) << '\n';
    }

    return description.str();
}

uint8_t SystemConfigurationOptionsEntry::get_option_count() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x05;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string SystemConfigurationOptionsEntry::get_option_at(size_t index) const
{
    return AbstractSMBiosEntry::dmi_string(index);
}
