#include <smbios/bios_information_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

BiosInformationEntry::BiosInformationEntry(const DMIHeader& header) {

    // TODO: check entry size
    if (header.type != SMBios::BIOSInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected BIOS Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();
}

BiosInformationEntry::~BiosInformationEntry()
{

}

std::string BiosInformationEntry::get_type() const
{
    return "BIOS Information";
}

std::string BiosInformationEntry::render_to_description() const
{
    return "IMPLEMENT ME!";
}

uint16_t BiosInformationEntry::get_starting_address() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->starting_segment;
}

uint8_t BiosInformationEntry::get_rom_size() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }

    // see extended_rom_size
    if (0xFF == bios_information24_->rom_size) {
        return 0;
    }

    return (0x10000 - bios_information24_->rom_size) << 4;
}

uint64_t BiosInformationEntry::get_properties() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->bios_properties;
}

uint8_t BiosInformationEntry::get_properties_extension1() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->properties_extension1;
}

uint8_t BiosInformationEntry::get_properties_extension2() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->properties_extension2;
}

uint8_t BiosInformationEntry::get_bios_major_release() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->bios_major_version;
}

uint8_t BiosInformationEntry::get_bios_minor_release() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->bios_minor_version;
}

uint8_t BiosInformationEntry::get_firmware_major_release() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->firmware_major_version;
}

uint8_t BiosInformationEntry::get_firmware_minor_release() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->firmware_minor_version;
}

void BiosInformationEntry::init_string_values()
{
        //"BIOS characteristics not supported", /* 3 */
        //"ISA is supported",
        //"MCA is supported",
        //"EISA is supported",
        //"PCI is supported",
        //"PC Card (PCMCIA) is supported",
        //"PNP is supported",
        //"APM is supported",
        //"BIOS is upgradeable",
        //"BIOS shadowing is allowed",
        //"VLB is supported",
        //"ESCD support is available",
        //"Boot from CD is supported",
        //"Selectable boot is supported",
        //"BIOS ROM is socketed",
        //"Boot from PC Card (PCMCIA) is supported",
        //"EDD is supported",
        //"Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)",
        //"Japanese floppy for Toshiba 1.2 MB is supported (int 13h)",
        //"5.25\"/360 kB floppy services are supported (int 13h)",
        //"5.25\"/1.2 MB floppy services are supported (int 13h)",
        //"3.5\"/720 kB floppy services are supported (int 13h)",
        //"3.5\"/2.88 MB floppy services are supported (int 13h)",
        //"Print screen service is supported (int 5h)",
        //"8042 keyboard services are supported (int 9h)",
        //"Serial services are supported (int 14h)",
        //"Printer services are supported (int 17h)",
        //"CGA/mono video services are supported (int 10h)",
        //"NEC PC-98" /* 31 */

}

