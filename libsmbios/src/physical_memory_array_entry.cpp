#include <smbios/physical_memory_array_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>
#include <iomanip>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::PhysicalMemoryArray at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::PhysicalMemoryArray, PhysicalMemoryArrayEntry)

PhysicalMemoryArrayEntry::PhysicalMemoryArrayEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::PhysicalMemoryArray) {
        std::stringstream err;
        err << "Wrong entry type, expected Physical Memory Array, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length (plus the
    // 0x80000000 sentinel check named in the class doc comment), confirmed
    // against c-versions/dmidecode_linux's own dmi_decode() for type 16.
    init_string_values();
}

void PhysicalMemoryArrayEntry::init_string_values()
{
    location_map_[LocationOutOfSpec] = "OutOfSpec";
    location_map_[LocationOther] = "Other";
    location_map_[LocationUnknown] = "Unknown";
    location_map_[SystemBoardOrMotherboard] = "System Board Or Motherboard";
    location_map_[ISAAddOnCard] = "ISA Add-on Card";
    location_map_[EISAAddOnCard] = "EISA Add-on Card";
    location_map_[PCIAddOnCard] = "PCI Add-on Card";
    location_map_[MCAAddOnCard] = "MCA Add-on Card";
    location_map_[PCMCIAAddOnCard] = "PCMCIA Add-on Card";
    location_map_[ProprietaryAddOnCard] = "Proprietary Add-on Card";
    location_map_[NuBus] = "NuBus";
    location_map_[PC98C20AddOnCard] = "PC-98/C20 Add-on Card";
    location_map_[PC98C24AddOnCard] = "PC-98/C24 Add-on Card";
    location_map_[PC98EAddOnCard] = "PC-98/E Add-on Card";
    location_map_[PC98LocalBusAddOnCard] = "PC-98/Local Bus Add-on Card";
    location_map_[CXLAddOnCard] = "CXL Add-on Card";

    use_map_[UseOutOfSpec] = "OutOfSpec";
    use_map_[UseOther] = "Other";
    use_map_[UseUnknown] = "Unknown";
    use_map_[SystemMemory] = "System Memory";
    use_map_[VideoMemory] = "Video Memory";
    use_map_[FlashMemory] = "Flash Memory";
    use_map_[NonVolatileRAM] = "Non-volatile RAM";
    use_map_[CacheMemory] = "Cache Memory";

    memory_error_correction_map_[MemoryErrorCorrectionOutOfSpec] = "OutOfSpec";
    memory_error_correction_map_[MemoryErrorCorrectionOther] = "Other";
    memory_error_correction_map_[MemoryErrorCorrectionUnknown] = "Unknown";
    memory_error_correction_map_[None] = "None";
    memory_error_correction_map_[Parity] = "Parity";
    memory_error_correction_map_[SingleBitECC] = "Single-bit ECC";
    memory_error_correction_map_[MultiBitECC] = "Multi-bit ECC";
    memory_error_correction_map_[CRC] = "CRC";
}

std::string PhysicalMemoryArrayEntry::get_type() const
{
    return "Physical Memory Array";
}

std::string PhysicalMemoryArrayEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Location: " << get_location_string() << '\n';
    description << "Use: " << get_use_string() << '\n';
    description << "Error Correction Type: " << get_memory_error_correction_string() << '\n';
    description << "Maximum Capacity: " << get_maximum_capacity_string() << '\n';
    description << "Error Information Handle: " << get_memory_error_information_handle_string() << '\n';
    description << "Number Of Devices: " << get_number_of_memory_devices_string() << '\n';

    return description.str();
}

uint8_t PhysicalMemoryArrayEntry::get_location() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return Location::LocationOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint8_t PhysicalMemoryArrayEntry::get_use() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return Use::UseOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint8_t PhysicalMemoryArrayEntry::get_memory_error_correction() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return MemoryErrorCorrection::MemoryErrorCorrectionOutOfSpec;
    return read_u8(raw_data(), offset);
}

uint64_t PhysicalMemoryArrayEntry::get_maximum_capacity() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;

    const uint32_t legacy_capacity = read_u32(raw_data(), offset);

    // 2.7+ widened field: see dmi_decode()'s own `DWORD(data + 0x07) ==
    // 0x80000000` sentinel check for case 16.
    constexpr uint32_t sentinel = 0x80000000;
    constexpr size_t extended_offset = 0x0F;
    constexpr size_t extended_min_length = 0x17;
    if (legacy_capacity == sentinel) {
        if (get_entry_size() >= extended_min_length) {
            return read_u64(raw_data(), extended_offset);
        }
        // Sentinel set but the structure is too short for the extended
        // field - the real capacity is genuinely unknown, not "fall back
        // to the sentinel value itself".
        return 0;
    }
    return static_cast<uint64_t>(legacy_capacity);
}

uint16_t PhysicalMemoryArrayEntry::get_memory_error_information_handle() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0xFFFE;
    return read_u16(raw_data(), offset);
}

uint16_t PhysicalMemoryArrayEntry::get_number_of_memory_devices() const
{
    constexpr size_t offset = 0x0D;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return 0;
    return read_u16(raw_data(), offset);
}

std::string PhysicalMemoryArrayEntry::get_location_string() const
{
    auto it = location_map_.find(get_location());
    if (it != location_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string PhysicalMemoryArrayEntry::get_use_string() const
{
    auto it = use_map_.find(get_use());
    if (it != use_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string PhysicalMemoryArrayEntry::get_memory_error_correction_string() const
{
    auto it = memory_error_correction_map_.find(get_memory_error_correction());
    if (it != memory_error_correction_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string PhysicalMemoryArrayEntry::get_maximum_capacity_string() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x0F;
    if (get_entry_size() < min_length) return "Unknown";

    const uint32_t legacy_capacity = read_u32(raw_data(), offset);
    constexpr uint32_t sentinel = 0x80000000;
    constexpr size_t extended_min_length = 0x17;

    if (legacy_capacity == sentinel && get_entry_size() < extended_min_length) {
        return "Unknown";
    }

    const bool extended = (legacy_capacity == sentinel);
    std::string capacity_string = std::to_string(get_maximum_capacity());
    capacity_string += extended ? " bytes" : " kB";
    return capacity_string;
}

std::string PhysicalMemoryArrayEntry::get_memory_error_information_handle_string() const
{
    const uint16_t handle = get_memory_error_information_handle();
    if (0xFFFE == handle) {
        return "Not Provided";
    }
    if (0xFFFF == handle) {
        return "No Error";
    }

    std::stringstream handle_stream;
    handle_stream << "0x" << std::hex << std::setfill('0') << std::setw(4) << handle << std::dec;
    return handle_stream.str();
}

std::string PhysicalMemoryArrayEntry::get_number_of_memory_devices_string() const
{
    const uint16_t count = get_number_of_memory_devices();
    if (0 == count) {
        return "Unknown";
    }
    return std::to_string(count);
}
