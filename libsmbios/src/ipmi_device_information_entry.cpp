#include <smbios/tables/ipmi_device_information_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::IpmiDeviceInformation at static-init time;
// see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h) for why
// this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::IpmiDeviceInformation, IpmiDeviceInformationEntry)

namespace {

constexpr size_t kInterfaceTypeOffset = 0x04;
constexpr size_t kSpecificationRevisionOffset = 0x05;
constexpr size_t kI2cTargetAddressOffset = 0x06;
constexpr size_t kNvStorageDeviceAddressOffset = 0x07;
constexpr size_t kBaseAddressOffset = 0x08;
constexpr size_t kBaseAddressModifierOffset = 0x10;
constexpr size_t kInterruptNumberOffset = 0x11;

constexpr size_t kInterfaceTypeMinLength = 0x05;
constexpr size_t kSpecificationRevisionMinLength = 0x06;
constexpr size_t kI2cTargetAddressMinLength = 0x07;
constexpr size_t kNvStorageDeviceAddressMinLength = 0x08;
constexpr size_t kBaseAddressMinLength = 0x10;
constexpr size_t kBaseAddressModifierMinLength = 0x11;
constexpr size_t kInterruptNumberMinLength = 0x12;

constexpr uint8_t kNvStorageDeviceAddressSentinel = 0xFF;
constexpr uint8_t kInterruptNumberSentinel = 0x00;

} // namespace

IpmiDeviceInformationEntry::IpmiDeviceInformationEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::IpmiDeviceInformation) {
        std::stringstream err;
        err << "Wrong entry type, expected IPMI Device Information, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.39/7.39.1 (Table
    // 114/115) - including the 0x10/0x11 redacted-gap resolution, see this
    // class's own header doc comment.
    init_string_values();
}

void IpmiDeviceInformationEntry::init_string_values()
{
    // DSP0134 3.9.0, Table 115.
    interface_type_map_[InterfaceTypeUnknown] = "Unknown";
    interface_type_map_[InterfaceTypeKcs] = "KCS: Keyboard Controller Style";
    interface_type_map_[InterfaceTypeSmic] = "SMIC: Server Management Interface Chip";
    interface_type_map_[InterfaceTypeBt] = "BT: Block Transfer";
    interface_type_map_[InterfaceTypeSsif] = "SSIF: SMBus System Interface";
}

std::string IpmiDeviceInformationEntry::get_type() const
{
    return "IPMI Device Information";
}

std::string IpmiDeviceInformationEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Interface Type: " << get_interface_type_string() << '\n';
    description << "Specification Revision: " << static_cast<unsigned>(get_specification_revision()) << '\n';
    description << "I2C Target Address: " << static_cast<unsigned>(get_i2c_target_address()) << '\n';
    description << "NV Storage Device Address: " << static_cast<unsigned>(get_nv_storage_device_address())
                << (has_nv_storage_device() ? "" : " (none)") << '\n';
    description << "Base Address: " << get_base_address()
                << (is_base_address_io_space() ? " (I/O space)" : " (memory-mapped)") << '\n';
    description << "Interrupt Number: " << static_cast<unsigned>(get_interrupt_number())
                << (has_interrupt() ? "" : " (unspecified/unsupported)") << '\n';

    return description.str();
}

IpmiDeviceInformationEntry::InterfaceType IpmiDeviceInformationEntry::get_interface_type() const
{
    if (get_entry_size() < kInterfaceTypeMinLength) return InterfaceTypeOutOfSpec;
    return static_cast<InterfaceType>(read_u8(raw_data(), kInterfaceTypeOffset));
}

std::string IpmiDeviceInformationEntry::get_interface_type_string() const
{
    auto it = interface_type_map_.find(get_interface_type());
    if (it != interface_type_map_.end()) {
        return (*it).second;
    }
    return "Reserved";
}

uint8_t IpmiDeviceInformationEntry::get_specification_revision_bcd() const
{
    if (get_entry_size() < kSpecificationRevisionMinLength) return 0;
    return read_u8(raw_data(), kSpecificationRevisionOffset);
}

uint8_t IpmiDeviceInformationEntry::get_specification_revision() const
{
    return bcd_to_decimal(get_specification_revision_bcd());
}

uint8_t IpmiDeviceInformationEntry::get_i2c_target_address() const
{
    if (get_entry_size() < kI2cTargetAddressMinLength) return 0;
    return read_u8(raw_data(), kI2cTargetAddressOffset);
}

uint8_t IpmiDeviceInformationEntry::get_nv_storage_device_address() const
{
    if (get_entry_size() < kNvStorageDeviceAddressMinLength) return kNvStorageDeviceAddressSentinel;
    return read_u8(raw_data(), kNvStorageDeviceAddressOffset);
}

bool IpmiDeviceInformationEntry::has_nv_storage_device() const
{
    return get_nv_storage_device_address() != kNvStorageDeviceAddressSentinel;
}

uint64_t IpmiDeviceInformationEntry::get_base_address() const
{
    if (get_entry_size() < kBaseAddressMinLength) return 0;
    return read_u64(raw_data(), kBaseAddressOffset);
}

bool IpmiDeviceInformationEntry::is_base_address_io_space() const
{
    return (get_base_address() & 0x1) != 0;
}

uint8_t IpmiDeviceInformationEntry::get_base_address_modifier_and_interrupt_info() const
{
    if (get_entry_size() < kBaseAddressModifierMinLength) return 0;
    return read_u8(raw_data(), kBaseAddressModifierOffset);
}

uint8_t IpmiDeviceInformationEntry::get_register_spacing() const
{
    return static_cast<uint8_t>((get_base_address_modifier_and_interrupt_info() >> 6) & 0x3);
}

bool IpmiDeviceInformationEntry::get_address_bit0() const
{
    return (get_base_address_modifier_and_interrupt_info() & (1u << 4)) != 0;
}

bool IpmiDeviceInformationEntry::has_interrupt_info() const
{
    return (get_base_address_modifier_and_interrupt_info() & (1u << 3)) != 0;
}

bool IpmiDeviceInformationEntry::get_interrupt_polarity_active_high() const
{
    return (get_base_address_modifier_and_interrupt_info() & (1u << 1)) != 0;
}

bool IpmiDeviceInformationEntry::get_interrupt_trigger_mode_level() const
{
    return (get_base_address_modifier_and_interrupt_info() & (1u << 0)) != 0;
}

uint8_t IpmiDeviceInformationEntry::get_interrupt_number() const
{
    if (get_entry_size() < kInterruptNumberMinLength) return kInterruptNumberSentinel;
    return read_u8(raw_data(), kInterruptNumberOffset);
}

bool IpmiDeviceInformationEntry::has_interrupt() const
{
    return get_interrupt_number() != kInterruptNumberSentinel;
}

uint8_t IpmiDeviceInformationEntry::bcd_to_decimal(uint8_t bcd)
{
    // Mechanical nibble-pair conversion only, same idiom as
    // SystemPowerControlsEntry::bcd_to_decimal() (7.26.1) - does not
    // validate that either nibble is itself a valid BCD digit.
    return static_cast<uint8_t>(((bcd >> 4) * 10) + (bcd & 0x0F));
}
