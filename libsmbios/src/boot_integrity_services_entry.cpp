#include <smbios/tables/boot_integrity_services_entry.h>
#include <smbios/smbios.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::BootIntegrityServices at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::BootIntegrityServices, BootIntegrityServicesEntry)

BootIntegrityServicesEntry::BootIntegrityServicesEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::BootIntegrityServices) {
        std::stringstream err;
        err << "Wrong entry type, expected Boot Integrity Services, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // DSP0134 3.9.0's 7.32 defines no fields for this structure - see this
    // class's own header doc comment. Nothing else to parse.
}

std::string BootIntegrityServicesEntry::get_type() const
{
    return "Boot Integrity Services Entry Point";
}

std::string BootIntegrityServicesEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "This structure carries no DSP0134-defined fields; it is reserved"
                    " for the external Boot Integrity Services API, which this project"
                    " does not implement." << '\n';

    return description.str();
}
