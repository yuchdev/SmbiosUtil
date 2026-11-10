#include <libsmbios/tables/inactive_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::Inactive at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::Inactive, InactiveEntry)

InactiveEntry::InactiveEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::Inactive) {
        std::stringstream err;
        err << "Wrong entry type, expected Inactive, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // DSP0134 3.9.0's 7.48 defines no fields for this structure beyond the
    // standard header - see this class's own header doc comment. Nothing
    // else to parse.
}

std::string InactiveEntry::get_type() const
{
    return "Inactive";
}

std::string InactiveEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "This structure's slot is currently inactive; per DSP0134 3.9.0 7.48,"
                    " upper-level software should bypass/skip it, just as it would any"
                    " structure type it does not recognize." << '\n';

    return description.str();
}

std::vector<NamedField> InactiveEntry::get_fields() const
{
    // Deliberately empty: the text output has no data fields (see header).
    return {};
}
