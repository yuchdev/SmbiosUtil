#include <smbios/system_slots_entry.h>
#include <smbios/smbios.h>
#include <smbios/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::SystemSlots at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemSlots, SystemSlotsEntry)

SystemSlotsEntry::SystemSlotsEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemSlots) {
        std::stringstream err;
        err << "Wrong entry type, expected System Slots, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone (R2),
    // confirmed against c-versions/dmidecode_linux's own dmi_decode() for
    // type 9.
    init_string_values();
}

void SystemSlotsEntry::init_string_values()
{
    // DMTF 7.10.1 slot-type table (dmi_slot_type()'s type[]/type_0x30[]/
    // type_0xA0[] arrays) - see class doc comment's scope note.
    slot_type_map_[0x01] = "Other";
    slot_type_map_[0x02] = "Unknown";
    slot_type_map_[0x03] = "ISA";
    slot_type_map_[0x04] = "MCA";
    slot_type_map_[0x05] = "EISA";
    slot_type_map_[0x06] = "PCI";
    slot_type_map_[0x07] = "PC Card (PCMCIA)";
    slot_type_map_[0x08] = "VLB";
    slot_type_map_[0x09] = "Proprietary";
    slot_type_map_[0x0A] = "Processor Card";
    slot_type_map_[0x0B] = "Proprietary Memory Card";
    slot_type_map_[0x0C] = "I/O Riser Card";
    slot_type_map_[0x0D] = "NuBus";
    slot_type_map_[0x0E] = "PCI-66";
    slot_type_map_[0x0F] = "AGP";
    slot_type_map_[0x10] = "AGP 2x";
    slot_type_map_[0x11] = "AGP 4x";
    slot_type_map_[0x12] = "PCI-X";
    slot_type_map_[0x13] = "AGP 8x";
    slot_type_map_[0x14] = "M.2 Socket 1-DP";
    slot_type_map_[0x15] = "M.2 Socket 1-SD";
    slot_type_map_[0x16] = "M.2 Socket 2";
    slot_type_map_[0x17] = "M.2 Socket 3";
    slot_type_map_[0x18] = "MXM Type I";
    slot_type_map_[0x19] = "MXM Type II";
    slot_type_map_[0x1A] = "MXM Type III";
    slot_type_map_[0x1B] = "MXM Type III-HE";
    slot_type_map_[0x1C] = "MXM Type IV";
    slot_type_map_[0x1D] = "MXM 3.0 Type A";
    slot_type_map_[0x1E] = "MXM 3.0 Type B";
    slot_type_map_[0x1F] = "PCI Express 2 SFF-8639 (U.2)";
    slot_type_map_[0x20] = "PCI Express 3 SFF-8639 (U.2)";
    slot_type_map_[0x21] = "PCI Express Mini 52-pin with bottom-side keep-outs";
    slot_type_map_[0x22] = "PCI Express Mini 52-pin without bottom-side keep-outs";
    slot_type_map_[0x23] = "PCI Express Mini 76-pin";
    slot_type_map_[0x24] = "PCI Express 4 SFF-8639 (U.2)";
    slot_type_map_[0x25] = "PCI Express 5 SFF-8639 (U.2)";
    slot_type_map_[0x26] = "OCP NIC 3.0 Small Form Factor (SFF)";
    slot_type_map_[0x27] = "OCP NIC 3.0 Large Form Factor (LFF)";
    slot_type_map_[0x28] = "OCP NIC Prior to 3.0";
    slot_type_map_[0x30] = "CXL FLexbus 1.0";
    slot_type_map_[0xA0] = "PC-98/C20";
    slot_type_map_[0xA1] = "PC-98/C24";
    slot_type_map_[0xA2] = "PC-98/E";
    slot_type_map_[0xA3] = "PC-98/Local Bus";
    slot_type_map_[0xA4] = "PC-98/Card";
    slot_type_map_[0xA5] = "PCI Express";
    slot_type_map_[0xA6] = "PCI Express x1";
    slot_type_map_[0xA7] = "PCI Express x2";
    slot_type_map_[0xA8] = "PCI Express x4";
    slot_type_map_[0xA9] = "PCI Express x8";
    slot_type_map_[0xAA] = "PCI Express x16";
    slot_type_map_[0xAB] = "PCI Express 2";
    slot_type_map_[0xAC] = "PCI Express 2 x1";
    slot_type_map_[0xAD] = "PCI Express 2 x2";
    slot_type_map_[0xAE] = "PCI Express 2 x4";
    slot_type_map_[0xAF] = "PCI Express 2 x8";
    slot_type_map_[0xB0] = "PCI Express 2 x16";
    slot_type_map_[0xB1] = "PCI Express 3";
    slot_type_map_[0xB2] = "PCI Express 3 x1";
    slot_type_map_[0xB3] = "PCI Express 3 x2";
    slot_type_map_[0xB4] = "PCI Express 3 x4";
    slot_type_map_[0xB5] = "PCI Express 3 x8";
    slot_type_map_[0xB6] = "PCI Express 3 x16";
    // 0xB7 is deliberately absent from the reference's own type_0xA0[]
    // table too (it holds the literal out_of_spec placeholder there) - the
    // lookup's own "OutOfSpec" fallback below covers it correctly.
    slot_type_map_[0xB8] = "PCI Express 4";
    slot_type_map_[0xB9] = "PCI Express 4 x1";
    slot_type_map_[0xBA] = "PCI Express 4 x2";
    slot_type_map_[0xBB] = "PCI Express 4 x4";
    slot_type_map_[0xBC] = "PCI Express 4 x8";
    slot_type_map_[0xBD] = "PCI Express 4 x16";
    slot_type_map_[0xBE] = "PCI Express 5";
    slot_type_map_[0xBF] = "PCI Express 5 x1";
    slot_type_map_[0xC0] = "PCI Express 5 x2";
    slot_type_map_[0xC1] = "PCI Express 5 x4";
    slot_type_map_[0xC2] = "PCI Express 5 x8";
    slot_type_map_[0xC3] = "PCI Express 5 x16";
    slot_type_map_[0xC4] = "PCI Express 6+";
    slot_type_map_[0xC5] = "EDSFF E1";
    slot_type_map_[0xC6] = "EDSFF E3";

    data_bus_width_map_[DataBusWidthOutOfSpec] = "OutOfSpec";
    data_bus_width_map_[DataBusWidthOther] = "Other";
    data_bus_width_map_[DataBusWidthUnknown] = "Unknown";
    data_bus_width_map_[DataBusWidth8Bit] = "8 bit";
    data_bus_width_map_[DataBusWidth16Bit] = "16 bit";
    data_bus_width_map_[DataBusWidth32Bit] = "32 bit";
    data_bus_width_map_[DataBusWidth64Bit] = "64 bit";
    data_bus_width_map_[DataBusWidth128Bit] = "128 bit";
    data_bus_width_map_[DataBusWidth1xOrX1] = "1x or x1";
    data_bus_width_map_[DataBusWidth2xOrX2] = "2x or x2";
    data_bus_width_map_[DataBusWidth4xOrX4] = "4x or x4";
    data_bus_width_map_[DataBusWidth8xOrX8] = "8x or x8";
    data_bus_width_map_[DataBusWidth12xOrX12] = "12x or x12";
    data_bus_width_map_[DataBusWidth16xOrX16] = "16x or x16";
    data_bus_width_map_[DataBusWidth32xOrX32] = "32x or x32";

    current_usage_map_[CurrentUsageOutOfSpec] = "OutOfSpec";
    current_usage_map_[CurrentUsageOther] = "Other";
    current_usage_map_[CurrentUsageUnknown] = "Unknown";
    current_usage_map_[Available] = "Available";
    current_usage_map_[InUse] = "In Use";
    current_usage_map_[Unavailable] = "Unavailable";

    slot_length_map_[SlotLengthOutOfSpec] = "OutOfSpec";
    slot_length_map_[SlotLengthOther] = "Other";
    slot_length_map_[SlotLengthUnknown] = "Unknown";
    slot_length_map_[Short] = "Short";
    slot_length_map_[Long] = "Long";
    slot_length_map_[DriveFormFactor2Dot5Inch] = "2.5\" drive form factor";
    slot_length_map_[DriveFormFactor3Dot5Inch] = "3.5\" drive form factor";

    characteristics1_map_[Provides5V] = "5.0 V is provided";
    characteristics1_map_[Provides3V3] = "3.3 V is provided";
    characteristics1_map_[OpeningShared] = "Opening is shared";
    characteristics1_map_[PCCard16Supported] = "PC Card-16 is supported";
    characteristics1_map_[CardBusSupported] = "Cardbus is supported";
    characteristics1_map_[ZoomVideoSupported] = "Zoom Video is supported";
    characteristics1_map_[ModemRingResumeSupported] = "Modem ring resume is supported";

    characteristics2_map_[PMESignalSupported] = "PME signal is supported";
    characteristics2_map_[HotPlugDevicesSupported] = "Hot-plug devices are supported";
    characteristics2_map_[SMBusSignalSupported] = "SMBus signal is supported";
    characteristics2_map_[PCIeSlotBifurcationSupported] = "PCIe slot bifurcation is supported";
    characteristics2_map_[AsyncSurpriseRemovalSupported] = "Async/surprise removal is supported";
    characteristics2_map_[Flexbus1Dot0Capable] = "Flexbus slot, CXL 1.0 capable";
    characteristics2_map_[Flexbus2Dot0Capable] = "Flexbus slot, CXL 2.0 capable";
    characteristics2_map_[Flexbus3Dot0Capable] = "Flexbus slot, CXL 3.0 capable";
}

std::string SystemSlotsEntry::get_type() const
{
    return "System Slots";
}

std::string SystemSlotsEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Designation: " << get_designation_string() << '\n';
    description << "Type: " << get_slot_type_string() << '\n';
    description << "Data Bus Width: " << get_data_bus_width_string() << '\n';
    description << "Current Usage: " << get_current_usage_string() << '\n';
    description << "Length: " << get_slot_length_string() << '\n';
    description << "ID: " << get_slot_id_string() << '\n';
    description << "Characteristics: " << '\n' << get_characteristics_string();
    description << "Bus Address: " << get_bus_address_string() << '\n';

    return description.str();
}

uint8_t SystemSlotsEntry::get_designation_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_slot_type() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_data_bus_width() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return DataBusWidth::DataBusWidthOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_current_usage() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return CurrentUsage::CurrentUsageOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_slot_length() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return SlotLength::SlotLengthOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_slot_id_low() const
{
    constexpr size_t offset = 0x09;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_slot_id_high() const
{
    constexpr size_t offset = 0x0A;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_characteristics1() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0C;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_characteristics2() const
{
    constexpr size_t offset = 0x0C;
    constexpr size_t min_length = 0x0D;

    // See dmi_slot_characteristics()'s own `if (h->length < 0x0D) ...
    // data[0x0B], 0x00) else ... data[0x0B], data[0x0C]` branch: this byte
    // always participates, falling back to 0 rather than being skipped
    // when the structure is too short to actually carry it.
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint16_t SystemSlotsEntry::get_segment_group_number() const
{
    constexpr size_t offset = 0x0D;
    constexpr size_t min_length = 0x11;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_bus_number() const
{
    constexpr size_t offset = 0x0F;
    constexpr size_t min_length = 0x11;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_device_function_number() const
{
    constexpr size_t offset = 0x10;
    constexpr size_t min_length = 0x11;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemSlotsEntry::get_device_number() const
{
    return static_cast<uint8_t>(get_device_function_number() >> 3);
}

uint8_t SystemSlotsEntry::get_function_number() const
{
    return static_cast<uint8_t>(get_device_function_number() & 0x07);
}

std::string SystemSlotsEntry::get_designation_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_designation_index());
}

std::string SystemSlotsEntry::get_slot_type_string() const
{
    auto it = slot_type_map_.find(get_slot_type());
    if (it != slot_type_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string SystemSlotsEntry::get_data_bus_width_string() const
{
    auto it = data_bus_width_map_.find(get_data_bus_width());
    if (it != data_bus_width_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string SystemSlotsEntry::get_current_usage_string() const
{
    auto it = current_usage_map_.find(get_current_usage());
    if (it != current_usage_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string SystemSlotsEntry::get_slot_length_string() const
{
    auto it = slot_length_map_.find(get_slot_length());
    if (it != slot_length_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string SystemSlotsEntry::get_slot_id_string() const
{
    const uint8_t low = get_slot_id_low();
    const uint8_t high = get_slot_id_high();
    if (0 == high) {
        return std::to_string(static_cast<unsigned>(low));
    }
    return std::to_string(static_cast<unsigned>(low)) + "/" + std::to_string(static_cast<unsigned>(high));
}

std::string SystemSlotsEntry::get_characteristics_string() const
{
    const uint8_t characteristics1 = get_characteristics1();
    const uint8_t characteristics2 = get_characteristics2();

    // See dmi_slot_characteristics(): bit 0 of characteristics1 set means
    // the whole field is unknown; both bytes otherwise entirely zero means
    // "None".
    if (characteristics1 & Characteristics1Unknown) {
        return "\tUnknown\n";
    }
    if ((characteristics1 & 0xFE) == 0 && characteristics2 == 0) {
        return "\tNone\n";
    }

    std::stringstream properties;
    properties << AbstractSMBiosEntry::bitset_to_properties<uint8_t>(characteristics1, characteristics1_map_);
    properties << AbstractSMBiosEntry::bitset_to_properties<uint8_t>(characteristics2, characteristics2_map_);
    return properties.str();
}

std::string SystemSlotsEntry::get_bus_address_string() const
{
    constexpr size_t min_length = 0x11;
    if (get_entry_size() < min_length) return "";

    const uint16_t segment = get_segment_group_number();
    const uint8_t bus = get_bus_number();
    const uint8_t device_function = get_device_function_number();

    // See dmi_slot_segment_bus_func()'s own `!(code1 == 0xFFFF && code2 ==
    // 0xFF && code3 == 0xFF)` guard - all-FF/all-FFFF means "not
    // applicable", not a real address.
    if (segment == 0xFFFF && bus == 0xFF && device_function == 0xFF) {
        return "";
    }

    std::stringstream address;
    address << std::hex << std::setfill('0')
            << std::setw(4) << segment << ':'
            << std::setw(2) << static_cast<unsigned>(bus) << ':'
            << std::setw(2) << static_cast<unsigned>(get_device_number()) << '.'
            << std::dec << static_cast<unsigned>(get_function_number());
    return address.str();
}
