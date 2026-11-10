#include <libsmbios/tables/pointing_device_entry.hpp>
#include <libsmbios/smbios.hpp>
#include <smbios/sys_interface/byte_reader.hpp>
#include <libsmbios/smbios_entry_factory.hpp>

#include <sstream>
#include <cassert>

using namespace smbios;

// Self-register against SMBios::PointingDevice at static-init time; see
// REGISTER_SMBIOS_ENTRY's doc comment (smbios_entry_factory.hpp) for why this
// lives here rather than in the header.
REGISTER_SMBIOS_ENTRY(SMBios::PointingDevice, PointingDeviceEntry)

namespace {

constexpr size_t kDeviceTypeOffset = 0x04;
constexpr size_t kInterfaceOffset = 0x05;
constexpr size_t kButtonCountOffset = 0x06;

constexpr size_t kDeviceTypeMinLength = 0x05;
constexpr size_t kInterfaceMinLength = 0x06;
constexpr size_t kButtonCountMinLength = 0x07;

} // namespace

PointingDeviceEntry::PointingDeviceEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
    : AbstractSMBiosEntry(header, table_end)
{
    if (header.type != SMBios::PointingDevice) {
        std::stringstream err;
        err << "Wrong entry type, expected Built-in Pointing Device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    // Every field below is distinguished by header.length alone, confirmed
    // against docs/standard/DSP0134_3.9.0.md's own 7.22/7.22.1/7.22.2.
    init_string_values();
}

void PointingDeviceEntry::init_string_values()
{
    // DSP0134 3.9.0, 7.22.1 Table 88.
    device_type_map_[Other] = "Other";
    device_type_map_[Unknown] = "Unknown";
    device_type_map_[Mouse] = "Mouse";
    device_type_map_[TrackBall] = "Track Ball";
    device_type_map_[TrackPoint] = "Track Point";
    device_type_map_[GlidePoint] = "Glide Point";
    device_type_map_[TouchPad] = "Touch Pad";
    device_type_map_[TouchScreen] = "Touch Screen";
    device_type_map_[OpticalSensor] = "Optical Sensor";

    // DSP0134 3.9.0, 7.22.2 Table 89.
    interface_map_[InterfaceOther] = "Other";
    interface_map_[InterfaceUnknown] = "Unknown";
    interface_map_[Serial] = "Serial";
    interface_map_[PS2] = "PS/2";
    interface_map_[Infrared] = "Infrared";
    interface_map_[HPHIL] = "HP-HIL";
    interface_map_[BusMouse] = "Bus mouse";
    interface_map_[ADB] = "ADB (Apple Desktop Bus)";
    interface_map_[BusMouseDB9] = "Bus mouse DB-9";
    interface_map_[BusMouseMicroDIN] = "Bus mouse micro-DIN";
    interface_map_[USB] = "USB";
    interface_map_[I2C] = "I2C";
    interface_map_[SPI] = "SPI";
}

std::string PointingDeviceEntry::get_type() const
{
    return "Built-in Pointing Device";
}

std::string PointingDeviceEntry::render_to_description() const
{
    std::stringstream description;
    description << "Header type: " << get_type() << '\n';
    description << "Device Type: " << get_device_type_string() << '\n';
    description << "Interface: " << get_interface_string() << '\n';
    description << "Number of Buttons: " << static_cast<unsigned>(get_button_count()) << '\n';

    return description.str();
}

uint8_t PointingDeviceEntry::get_device_type() const
{
    if (get_entry_size() < kDeviceTypeMinLength) return DeviceTypeOutOfSpec;

    const uint8_t device_type = read_u8(raw_data(), kDeviceTypeOffset);
    auto it = device_type_map_.find(device_type);
    if (it != device_type_map_.end()) {
        return (*it).first;
    }
    return DeviceTypeOutOfSpec;
}

uint8_t PointingDeviceEntry::get_interface() const
{
    if (get_entry_size() < kInterfaceMinLength) return InterfaceOutOfSpec;

    const uint8_t interface_value = read_u8(raw_data(), kInterfaceOffset);
    auto it = interface_map_.find(interface_value);
    if (it != interface_map_.end()) {
        return (*it).first;
    }
    return InterfaceOutOfSpec;
}

uint8_t PointingDeviceEntry::get_button_count() const
{
    if (get_entry_size() < kButtonCountMinLength) return 0;
    return read_u8(raw_data(), kButtonCountOffset);
}

std::string PointingDeviceEntry::get_device_type_string() const
{
    assert(!device_type_map_.empty());
    auto it = device_type_map_.find(get_device_type());
    if (it != device_type_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::string PointingDeviceEntry::get_interface_string() const
{
    assert(!interface_map_.empty());
    auto it = interface_map_.find(get_interface());
    if (it != interface_map_.end()) {
        return (*it).second;
    }
    return "OutOfSpec";
}

std::vector<NamedField> PointingDeviceEntry::get_fields() const
{
    std::vector<NamedField> fields;
    fields.push_back({"Device Type", get_device_type_string()});
    fields.push_back({"Interface", get_interface_string()});
    fields.push_back({"Number of Buttons", static_cast<uint64_t>(get_button_count())});
    return fields;
}
