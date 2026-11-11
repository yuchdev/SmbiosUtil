#include <libsmbios/tables/system_enclosure_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::SystemEnclosure at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::SystemEnclosure, SystemEnclosureEntry)

SystemEnclosureEntry::SystemEnclosureEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::SystemEnclosure) {
        std::stringstream err;
        err << "Wrong entry type, expected System Enclosure, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone (matches
    // this project's established convention for prior entry classes);
    // confirmed against c-versions/dmidecode_linux's own dmi_decode() for
    // type 3, which also gates purely on h->length.
    init_string_values();
}

void SystemEnclosureEntry::init_string_values()
{
    chassis_type_map_[ChassisTypeOutOfSpec] = "OutOfSpec";
    chassis_type_map_[ChassisTypeOther] = "Other";
    chassis_type_map_[ChassisTypeUnknown] = "Unknown";
    chassis_type_map_[Desktop] = "Desktop";
    chassis_type_map_[LowProfileDesktop] = "Low Profile Desktop";
    chassis_type_map_[PizzaBox] = "Pizza Box";
    chassis_type_map_[MiniTower] = "Mini Tower";
    chassis_type_map_[Tower] = "Tower";
    chassis_type_map_[Portable] = "Portable";
    chassis_type_map_[Laptop] = "Laptop";
    chassis_type_map_[Notebook] = "Notebook";
    chassis_type_map_[HandHeld] = "Hand Held";
    chassis_type_map_[DockingStation] = "Docking Station";
    chassis_type_map_[AllInOne] = "All In One";
    chassis_type_map_[SubNotebook] = "Sub Notebook";
    chassis_type_map_[SpaceSaving] = "Space-saving";
    chassis_type_map_[LunchBox] = "Lunch Box";
    chassis_type_map_[MainServerChassis] = "Main Server Chassis";
    chassis_type_map_[ExpansionChassis] = "Expansion Chassis";
    chassis_type_map_[SubChassis] = "Sub Chassis";
    chassis_type_map_[BusExpansionChassis] = "Bus Expansion Chassis";
    chassis_type_map_[PeripheralChassis] = "Peripheral Chassis";
    chassis_type_map_[RAIDChassis] = "RAID Chassis";
    chassis_type_map_[RackMountChassis] = "Rack Mount Chassis";
    chassis_type_map_[SealedCasePC] = "Sealed-case PC";
    chassis_type_map_[MultiSystem] = "Multi-system";
    chassis_type_map_[CompactPCI] = "CompactPCI";
    chassis_type_map_[AdvancedTCA] = "AdvancedTCA";
    chassis_type_map_[Blade] = "Blade";
    chassis_type_map_[BladeEnclosing] = "Blade Enclosing";
    chassis_type_map_[Tablet] = "Tablet";
    chassis_type_map_[Convertible] = "Convertible";
    chassis_type_map_[Detachable] = "Detachable";
    chassis_type_map_[IoTGateway] = "IoT Gateway";
    chassis_type_map_[EmbeddedPC] = "Embedded PC";
    chassis_type_map_[MiniPC] = "Mini PC";
    chassis_type_map_[StickPC] = "Stick PC";

    chassis_state_map_[ChassisStateOutOfSpec] = "OutOfSpec";
    chassis_state_map_[ChassisStateOther] = "Other";
    chassis_state_map_[ChassisStateUnknown] = "Unknown";
    chassis_state_map_[Safe] = "Safe";
    chassis_state_map_[Warning] = "Warning";
    chassis_state_map_[Critical] = "Critical";
    chassis_state_map_[NonRecoverable] = "Non-recoverable";

    security_status_map_[SecurityStatusOutOfSpec] = "OutOfSpec";
    security_status_map_[SecurityStatusOther] = "Other";
    security_status_map_[SecurityStatusUnknown] = "Unknown";
    security_status_map_[None] = "None";
    security_status_map_[ExternalInterfaceLockedOut] = "External Interface Locked Out";
    security_status_map_[ExternalInterfaceEnabled] = "External Interface Enabled";
}

std::string SystemEnclosureEntry::get_type() const
{
    return "System Enclosure";
}

std::string SystemEnclosureEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Manufacturer: " << get_manufacturer_string() << '\n';
    description << "Type: " << get_enclosure_type_string() << '\n';
    description << "Lock: " << get_lock_present_string() << '\n';
    description << "Version: " << get_version_string() << '\n';
    description << "Serial Number: " << get_serial_number_string() << '\n';
    description << "Asset Tag: " << get_asset_tag_string() << '\n';
    description << "Boot-up State: " << get_boot_up_state_string() << '\n';
    description << "Power Supply State: " << get_power_supply_state_string() << '\n';
    description << "Thermal State: " << get_thermal_state_string() << '\n';
    description << "Security Status: " << get_security_status_string() << '\n';
    description << "OEM Information: " << get_oem_defined_string() << '\n';
    description << "Height: " << get_height_string() << '\n';
    description << "Number Of Power Cords: " << get_number_of_power_cords_string() << '\n';
    description << "Contained Elements: " << static_cast<unsigned>(get_contained_element_count()) << '\n';
    description << "SKU Number: " << get_sku_number_string() << '\n';

    return description.str();
}

uint8_t SystemEnclosureEntry::get_manufacturer_index() const
{
    constexpr size_t offset = 0x04;
    constexpr size_t min_length = 0x09; // manufacturer..asset_tag gated together (dmidecode.c case 3)
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_enclosure_type() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return ChassisType::ChassisTypeOutOfSpec;

    const uint8_t enclosure_type = read_u8(raw_data(), offset) & 0x7F;
    auto it = chassis_type_map_.find(enclosure_type);
    if (it != chassis_type_map_.end()) {
        return (*it).first;
    }
    return ChassisType::ChassisTypeOutOfSpec;
}

bool SystemEnclosureEntry::get_lock_present() const
{
    constexpr size_t offset = 0x05;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return false;

    return (read_u8(raw_data(), offset) & 0x80) != 0;
}

uint8_t SystemEnclosureEntry::get_version_index() const
{
    constexpr size_t offset = 0x06;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_serial_number_index() const
{
    constexpr size_t offset = 0x07;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_asset_tag_index() const
{
    constexpr size_t offset = 0x08;
    constexpr size_t min_length = 0x09;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_boot_up_state() const
{
    constexpr size_t offset = 0x09;
    constexpr size_t min_length = 0x0D; // boot_up_state..security_status gated together
    if (get_entry_size() < min_length) return ChassisState::ChassisStateOutOfSpec;

    const uint8_t state = read_u8(raw_data(), offset);
    auto it = chassis_state_map_.find(state);
    if (it != chassis_state_map_.end()) {
        return (*it).first;
    }
    return ChassisState::ChassisStateOutOfSpec;
}

uint8_t SystemEnclosureEntry::get_power_supply_state() const
{
    constexpr size_t offset = 0x0A;
    constexpr size_t min_length = 0x0D;
    if (get_entry_size() < min_length) return ChassisState::ChassisStateOutOfSpec;

    const uint8_t state = read_u8(raw_data(), offset);
    auto it = chassis_state_map_.find(state);
    if (it != chassis_state_map_.end()) {
        return (*it).first;
    }
    return ChassisState::ChassisStateOutOfSpec;
}

uint8_t SystemEnclosureEntry::get_thermal_state() const
{
    constexpr size_t offset = 0x0B;
    constexpr size_t min_length = 0x0D;
    if (get_entry_size() < min_length) return ChassisState::ChassisStateOutOfSpec;

    const uint8_t state = read_u8(raw_data(), offset);
    auto it = chassis_state_map_.find(state);
    if (it != chassis_state_map_.end()) {
        return (*it).first;
    }
    return ChassisState::ChassisStateOutOfSpec;
}

uint8_t SystemEnclosureEntry::get_security_status() const
{
    constexpr size_t offset = 0x0C;
    constexpr size_t min_length = 0x0D;
    if (get_entry_size() < min_length) return SecurityStatus::SecurityStatusOutOfSpec;

    const uint8_t status = read_u8(raw_data(), offset);
    auto it = security_status_map_.find(status);
    if (it != security_status_map_.end()) {
        return (*it).first;
    }
    return SecurityStatus::SecurityStatusOutOfSpec;
}

uint32_t SystemEnclosureEntry::get_oem_defined() const
{
    constexpr size_t offset = 0x0D;
    constexpr size_t min_length = 0x11;
    if (get_entry_size() < min_length) return 0;
    return read_u32(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_height() const
{
    constexpr size_t offset = 0x11;
    constexpr size_t min_length = 0x13; // height+number_of_power_cords gated together
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_number_of_power_cords() const
{
    constexpr size_t offset = 0x12;
    constexpr size_t min_length = 0x13;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_contained_element_count() const
{
    constexpr size_t offset = 0x13;
    constexpr size_t min_length = 0x15; // count+record_length gated together
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_contained_element_record_length() const
{
    constexpr size_t offset = 0x14;
    constexpr size_t min_length = 0x15;
    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

uint8_t SystemEnclosureEntry::get_sku_number_index() const
{
    // Runtime-computed offset: the contained-elements array (0x15,
    // count * record_length bytes) sits between the fixed-offset fields and
    // the SKU number, so there is no constexpr offset for this field - see
    // the class's own header doc comment and dmidecode.c case 3's
    // "if (h->length < 0x16 + data[0x13] * data[0x14]) break;" guard, which
    // this reproduces exactly.
    const size_t count = get_contained_element_count();
    const size_t record_length = get_contained_element_record_length();
    const size_t offset = 0x15 + count * record_length;
    const size_t min_length = offset + 1;

    if (get_entry_size() < min_length) return 0;
    return read_u8(raw_data(), offset);
}

std::string SystemEnclosureEntry::get_manufacturer_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_manufacturer_index());
}

std::string SystemEnclosureEntry::get_enclosure_type_string() const
{
    assert(!chassis_type_map_.empty());
    return (*chassis_type_map_.find(get_enclosure_type())).second;
}

std::string SystemEnclosureEntry::get_lock_present_string() const
{
    return get_lock_present() ? "Present" : "Not Present";
}

std::string SystemEnclosureEntry::get_version_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_version_index());
}

std::string SystemEnclosureEntry::get_serial_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_serial_number_index());
}

std::string SystemEnclosureEntry::get_asset_tag_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_asset_tag_index());
}

std::string SystemEnclosureEntry::chassis_state_string(uint8_t state) const
{
    assert(!chassis_state_map_.empty());
    return (*chassis_state_map_.find(state)).second;
}

std::string SystemEnclosureEntry::get_boot_up_state_string() const
{
    return chassis_state_string(get_boot_up_state());
}

std::string SystemEnclosureEntry::get_power_supply_state_string() const
{
    return chassis_state_string(get_power_supply_state());
}

std::string SystemEnclosureEntry::get_thermal_state_string() const
{
    return chassis_state_string(get_thermal_state());
}

std::string SystemEnclosureEntry::get_security_status_string() const
{
    assert(!security_status_map_.empty());
    return (*security_status_map_.find(get_security_status())).second;
}

std::string SystemEnclosureEntry::get_oem_defined_string() const
{
    std::stringstream oem_stream;
    oem_stream << std::hex << std::showbase << get_oem_defined() << std::dec << std::noshowbase;
    return oem_stream.str();
}

std::string SystemEnclosureEntry::get_height_string() const
{
    if (0 == get_height()) {
        return "Unspecified";
    }
    return std::to_string(static_cast<unsigned>(get_height())) + " U";
}

std::string SystemEnclosureEntry::get_number_of_power_cords_string() const
{
    if (0 == get_number_of_power_cords()) {
        return "Unspecified";
    }
    return std::to_string(static_cast<unsigned>(get_number_of_power_cords()));
}

std::string SystemEnclosureEntry::get_sku_number_string() const
{
    return AbstractSMBiosEntry::dmi_string(get_sku_number_index());
}

std::vector<NamedField> SystemEnclosureEntry::get_fields() const
{
    std::vector<NamedField> fields;
    fields.push_back({"Manufacturer", get_manufacturer_string()});
    fields.push_back({"Type", get_enclosure_type_string()});
    fields.push_back({"Lock", get_lock_present()});
    fields.push_back({"Version", get_version_string()});
    fields.push_back({"Serial Number", get_serial_number_string()});
    fields.push_back({"Asset Tag", get_asset_tag_string()});
    fields.push_back({"Boot-up State", get_boot_up_state_string()});
    fields.push_back({"Power Supply State", get_power_supply_state_string()});
    fields.push_back({"Thermal State", get_thermal_state_string()});
    fields.push_back({"Security Status", get_security_status_string()});
    fields.push_back({"OEM Information", get_oem_defined_string()});
    fields.push_back({"Height", get_height_string()});
    fields.push_back({"Number Of Power Cords", get_number_of_power_cords_string()});
    fields.push_back({"Contained Elements", static_cast<uint64_t>(get_contained_element_count())});
    fields.push_back({"SKU Number", get_sku_number_string()});
    return fields;
}
