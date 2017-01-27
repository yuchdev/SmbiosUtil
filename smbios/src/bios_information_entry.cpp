#include <smbios/bios_information_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

BiosInformationEntry::BiosInformationEntry(const DMIHeader& header, const SMBiosVersion& version) 
    : AbstractSMBiosEntry(header) 
{
    if (header.type != SMBios::BIOSInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected BIOS Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    init_string_values();

    // check empty entry
    if ((header.length < 0x12) || (version < SMBiosVersion{2, 1}))
        return;

    if ((header.length >= 0x12) && (version > SMBiosVersion{ 2, 3 })) {
        bios_information24_ = reinterpret_cast<const BiosInformationV24*>(header.data);
    }

    if ((header.length > 0x18) && (version > SMBiosVersion{ 3, 0 })){
        bios_information31_ = reinterpret_cast<const BiosInformationV31*>(header.data);
    }
}

std::string BiosInformationEntry::get_type() const
{
    return "BIOS Information";
}

std::string BiosInformationEntry::render_to_description() const
{
    std::stringstream decsription;
    decsription << "Header type: " << get_type() << '\n';
    decsription << "Vendor: " << get_vendor_string() << '\n';
    decsription << "Version: " << get_version_string() << '\n';
    decsription << "Address: " << get_starting_address_string() << '\n';
    decsription << "Runtime size: " << get_runtime_size_string() << '\n';
    decsription << "Release Date: " << get_release_date_string() << '\n';
    decsription << "ROM Size: " << get_rom_size_string() << '\n';
    decsription << "BIOS properties: " << '\n' << get_properties_string();
    decsription << "BIOS properties extend1: " << '\n' << get_properties_extension1_string();
    decsription << "BIOS properties extend2: " << '\n' << get_properties_extension2_string();
    decsription << "BIOS Revision: " << get_bios_version_string() << '\n';
    decsription << "Firmware Revision: " << get_firmware_version_string() << '\n';

    return std::move(decsription.str());
}

uint8_t BiosInformationEntry::get_vendor_index() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->vendor;
}

uint8_t BiosInformationEntry::get_version_index() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->bios_version;
}

uint16_t BiosInformationEntry::get_starting_address() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->starting_segment;
}

uint32_t BiosInformationEntry::get_runtime_size() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return static_cast<uint32_t>((0x10000 - bios_information24_->starting_segment) << 4);
}

uint8_t BiosInformationEntry::get_release_date_index() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }
    return bios_information24_->release_date;
}

uint8_t BiosInformationEntry::get_rom_size() const
{
    if (nullptr == bios_information24_) {
        return 0;
    }

    // see extended_rom_size then
    if (0xFF == bios_information24_->rom_size) {
        return 0;
    }

    return (bios_information24_->rom_size + 1) << 6;
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
    properties_map_[BiosPropertiesOutOfSpec] = "OutOfSpec";
    properties_map_[Reserved1] = "Reserved";
    properties_map_[Reserved2] = "Reserved";
    properties_map_[Unknown] = "Unknown";
    properties_map_[NotSupported] = "BIOS characteristics not supported";
    properties_map_[ISASupported] = "ISA is supported";
    properties_map_[MCASupported] = "MCA is supported";
    properties_map_[EISASupported] = "EISA is supported";
    properties_map_[PCISupported] = "PCI is supported";
    properties_map_[PCMCIASupported] = "PC Card (PCMCIA) is supported";
    properties_map_[PnPSupported] = "PNP is supported";
    properties_map_[APMSupported] = "APM is supported";
    properties_map_[BIOSUpgradeable] = "BIOS is upgradeable";
    properties_map_[BIOSShadowingAllowed] = "BIOS shadowing is allowed";
    properties_map_[VLVESASupported] = "VLB is supported";
    properties_map_[ESCDSupported] = "ESCD support is available";
    properties_map_[BootFromCDSupported] = "Boot from CD is supported";
    properties_map_[SelectableBootSupported] = "Selectable boot is supported";
    properties_map_[BIOSROMSocketed] = "BIOS ROM is socketed";
    properties_map_[BootFromPCMCIASupported] = "Boot from PC Card (PCMCIA) is supported";
    properties_map_[EDDSpecificationSupported] = "EDD is supported";
    properties_map_[FloppyNECSupported] = "Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)";
    properties_map_[FloppyToshibaSupported] = "Japanese floppy for Toshiba 1.2 MB is supported (int 13h)";
    properties_map_[Floppy360kSupported] = "5.25\"/360 kB floppy services are supported (int 13h)";
    properties_map_[Floppy12MSupported] = "5.25\"/1.2 MB floppy services are supported (int 13h)";
    properties_map_[Floppy720kSupported] = "3.5\"/720 kB floppy services are supported (int 13h)";
    properties_map_[Floppy28MSupported] = "3.5\"/2.88 MB floppy services are supported (int 13h)";
    properties_map_[PrintScreenSupported] = "Print screen service is supported (int 5h)";
    properties_map_[KeyboardServicesSupported] = "8042 keyboard services are supported (int 9h)";
    properties_map_[SerialServicesSupported] = "Serial services are supported (int 14h)";
    properties_map_[PrinterServicesSupported] = "Printer services are supported (int 17h)";
    properties_map_[MonoVideoSupported] = "CGA/mono video services are supported (int 10h)";
    properties_map_[NECPC] = "NEC PC-98";

    properties_extensions1_map_[BiosPropertiesEx1OutOfSpec] = "OutOfSpec";
    properties_extensions1_map_[ACPISupported] = "ACPI is supported";
    properties_extensions1_map_[USBLegacySupported] = "USB Legacy is supported";
    properties_extensions1_map_[AGPSupported] = "AGP is supported";
    properties_extensions1_map_[I2OBootSupported] = "I2O boot is supported";
    properties_extensions1_map_[SuperDiskBootSupported] = "LS-120 SuperDisk boot is supported";
    properties_extensions1_map_[ZIPDriveBootSupported] = "ATAPI ZIP drive boot is supported";
    properties_extensions1_map_[IEEE1394BootSupported] = "1394 boot is supported";
    properties_extensions1_map_[SmartBatterySupported] = "Smart battery is supported";

    properties_extensions2_map_[BiosPropertiesEx2OutOfSpec] = "OutOfSpec";
    properties_extensions2_map_[BootSpecificationSupported] = "BIOS Boot Specification is supported";
    properties_extensions2_map_[KeyInitiatedNetworkBoot] = "Function key-initiated network service boot is supported";
    properties_extensions2_map_[TargetedContentDistribution] = "Enable targeted content distribution";
    properties_extensions2_map_[UEFISpecificationSupported] = "UEFI Specification is supported";
    properties_extensions2_map_[VirtualMachine] = "SMBIOS table describes a virtual machine";
}


std::string BiosInformationEntry::get_vendor_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_vendor_index());
}

std::string BiosInformationEntry::get_version_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_version_index());
}

std::string BiosInformationEntry::get_starting_address_string() const
{
    return AbstractSMBiosEntry::address_string(get_starting_address());
}

std::string BiosInformationEntry::get_release_date_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_release_date_index());
}

std::string BiosInformationEntry::get_rom_size_string() const
{
    string rom_size = std::to_string(static_cast<unsigned>(get_rom_size()));
    rom_size += " kB";
    return rom_size;
}

std::string BiosInformationEntry::get_runtime_size_string() const
{
    uint32_t runtime_size = get_runtime_size();
    string runtime_size_string;
    if (get_runtime_size() & 0x000003FF) {
        runtime_size_string = std::to_string(runtime_size);
        runtime_size_string += " bytes";
    }
    else {
        runtime_size_string = std::to_string(runtime_size >> 10);
        runtime_size_string += " kB";
    }
    return runtime_size_string;
}

std::string BiosInformationEntry::get_properties_string() const
{
    assert(!properties_map_.empty());
    const uint64_t properties = get_properties();
    return AbstractSMBiosEntry::bitset_to_properties<uint64_t>(properties, properties_map_);
}

std::string BiosInformationEntry::get_properties_extension1_string() const
{
    assert(!properties_extensions1_map_.empty());
    const uint8_t properties = get_properties_extension1();
    return AbstractSMBiosEntry::bitset_to_properties<uint8_t>(properties, properties_extensions1_map_);
}

std::string BiosInformationEntry::get_properties_extension2_string() const
{
    assert(!properties_extensions2_map_.empty());
    const uint8_t properties = get_properties_extension2();
    return AbstractSMBiosEntry::bitset_to_properties<uint8_t>(properties, properties_extensions2_map_);
}

std::string BiosInformationEntry::get_bios_version_string() const
{
    return stream_to_version(get_bios_major_release(), get_bios_minor_release());
}

std::string BiosInformationEntry::get_firmware_version_string() const
{
    return stream_to_version(get_firmware_major_release(), get_firmware_minor_release());
}

std::string BiosInformationEntry::stream_to_version(uint16_t major, uint16_t minor) const
{
    std::stringstream version_stream;
    version_stream << major << '.' << minor;
    return std::move(version_stream.str());
}

