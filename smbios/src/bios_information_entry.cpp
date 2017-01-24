#include <smbios/bios_information_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

BiosInformationEntry::BiosInformationEntry(const DMIHeader& header) {

    // TODO: check entry size
    if (header.type != SMBios::BiosInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected Bios Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();
}

BiosInformationEntry::~BiosInformationEntry()
{

}


