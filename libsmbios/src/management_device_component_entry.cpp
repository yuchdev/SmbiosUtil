#include <smbios/tables/management_device_component_entry.h>
#include <smbios/smbios.h>
#include <smbios/os_interface/byte_reader.h>
#include <smbios/smbios_entry_factory.h>

#include <sstream>

using namespace smbios;

// Self-register against SMBios::ManagementDeviceComponent at static-init
// time; see REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.h)
// for why this lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::ManagementDeviceComponent, ManagementDeviceComponentEntry)

namespace {

constexpr size_t kDescriptionOffset = 0x04;
constexpr size_t kManagementDeviceHandleOffset = 0x05;
constexpr size_t kComponentHandleOffset = 0x07;
constexpr size_t kThresholdHandleOffset = 0x09;

constexpr size_t kDescriptionMinLength = 0x05;
constexpr size_t kManagementDeviceHandleMinLength = 0x07;
constexpr size_t kComponentHandleMinLength = 0x09;
constexpr size_t kThresholdHandleMinLength = 0x0B;

} // namespace

ManagementDeviceComponentEntry::ManagementDeviceComponentEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::ManagementDeviceComponent) {
        std::stringstream err;
        err << "Wrong entry type, expected Management Device Component, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.36 (Table 110); see
    // this class's header comment for the Threshold Handle sentinel
    // confirmation.
}

std::string ManagementDeviceComponentEntry::get_type() const
{
    return "Management Device Component";
}

std::string ManagementDeviceComponentEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Description: " << get_description_string() << '\n';
    description << "Management Device Handle: " << get_management_device_handle() << '\n';
    description << "Component Handle: " << get_component_handle() << '\n';
    description << "Threshold Handle: "
                 << (has_threshold() ? std::to_string(get_threshold_handle()) : "None") << '\n';

    return description.str();
}

std::string ManagementDeviceComponentEntry::get_description_string() const
{
    if (get_entry_size() < kDescriptionMinLength) return std::string();
    return dmi_string(read_u8(raw_data(), kDescriptionOffset));
}

uint16_t ManagementDeviceComponentEntry::get_management_device_handle() const
{
    if (get_entry_size() < kManagementDeviceHandleMinLength) return 0;
    return read_u16(raw_data(), kManagementDeviceHandleOffset);
}

uint16_t ManagementDeviceComponentEntry::get_component_handle() const
{
    if (get_entry_size() < kComponentHandleMinLength) return 0;
    return read_u16(raw_data(), kComponentHandleOffset);
}

uint16_t ManagementDeviceComponentEntry::get_threshold_handle() const
{
    if (get_entry_size() < kThresholdHandleMinLength) return NoThresholdHandle;
    return read_u16(raw_data(), kThresholdHandleOffset);
}

bool ManagementDeviceComponentEntry::has_threshold() const
{
    return get_threshold_handle() != NoThresholdHandle;
}
