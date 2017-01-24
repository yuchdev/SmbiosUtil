#include <smbios/memory_device_entry.h>
#include <smbios/smbios.h>

#include <sstream>
#include <cassert>

using std::string;
using namespace smbios;

MemoryDeviceEntry::MemoryDeviceEntry(const DMIHeader& header) {

    if (header.type != SMBios::MemoryDevice) {
        std::stringstream err;
        err << "Wrong device type, expected Memory device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    if (header.length >= 0x15) {
        memory_device_v1_ = reinterpret_cast<const MemoryDeviceV21*>(header.data);
    }

    if (header.length >= 0x17) {
        memory_device_v2_ = reinterpret_cast<const MemoryDeviceV23*>(header.data);
    }

    if (header.length >= 0x1B) {
        memory_device_v3_ = reinterpret_cast<const MemoryDeviceV26*>(header.data);
    }

    if (header.length >= 0x1C) {
        memory_device_v4_ = reinterpret_cast<const MemoryDeviceV27*>(header.data);
    }

    init_string_values();
}

MemoryDeviceEntry::~MemoryDeviceEntry()
{
}

uint16_t MemoryDeviceEntry::get_array_handle() const
{
    if (nullptr == memory_device_v1_) {
        return 0;
    }

    return memory_device_v1_->array_handle;
}

uint16_t MemoryDeviceEntry::get_error_handle() const
{
    if (nullptr == memory_device_v1_) {
        return ErrorHandleValue::ErrorHandleNotProvided;
    }
    
    return memory_device_v1_->array_error_handle;
}

uint16_t MemoryDeviceEntry::get_total_width() const
{
    if (nullptr == memory_device_v1_) {
        return DataWidthValue::DataWidthUnknown1;
    }

    return memory_device_v1_->total_width;
}

uint16_t MemoryDeviceEntry::get_data_width() const
{
    if (nullptr == memory_device_v1_) {
        return DataWidthValue::DataWidthUnknown1;
    }

    return memory_device_v1_->data_width;
}

uint16_t MemoryDeviceEntry::get_device_size() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceSizeValue::DeviceSizeUnknown;
    }

    return memory_device_v1_->device_size;
}

uint8_t MemoryDeviceEntry::get_form_factor() const
{
    if (nullptr == memory_device_v1_) {
        return FormFactorValue::FormFactorOutOfSpec;
    }

    return memory_device_v1_->device_form_factor;
}

uint8_t MemoryDeviceEntry::get_device_set() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceSetValue::DeviceSetUnknown;
    }

    return memory_device_v1_->device_set;
}

uint8_t MemoryDeviceEntry::get_device_locator() const
{
    if (nullptr == memory_device_v1_) {
        return 0x0;
    }

    return memory_device_v1_->device_locator;
}

uint8_t MemoryDeviceEntry::get_bank_locator() const
{
    if (nullptr == memory_device_v1_) {
        return 0x0;
    }

    return memory_device_v1_->bank_locator;
}

uint8_t MemoryDeviceEntry::get_device_type() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceTypeValue::DeviceTypeOutOfSpec;
    }

    return memory_device_v1_->device_type;
}

uint16_t MemoryDeviceEntry::get_device_detail() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceProperties::DevicePropertiesOutOfSpec;
    }

    return memory_device_v1_->type_detail;
}

std::string MemoryDeviceEntry::get_type() const
{
    return "Memory Device";
}

std::string MemoryDeviceEntry::render_to_description() const
{
    std::stringstream decsription;
    decsription << "Array Handle: " << get_array_handle_string() << '\n';
    decsription << "Error Handle: " << get_error_handle_string() << '\n';
    decsription << "Total width: " << get_total_width_string() << '\n';
    decsription << "Data width: " << get_data_width_string() << '\n';
    decsription << "Device size: " << get_device_size_string() << '\n';
    decsription << "Form factor: " << get_form_factor_string() << '\n';
    decsription << "Device set: " << get_device_set_string() << '\n';
    decsription << "Device locator: " << get_device_locator_string() << '\n';
    decsription << "Bank locator: " << get_bank_locator_string() << '\n';
    decsription << "Device type: " << get_device_type_string() << '\n';
    decsription << "Device details: " << '\n' << get_device_detail_string() << '\n';

    return std::move(decsription.str());
}

std::string MemoryDeviceEntry::get_array_handle_string() const
{
    uint16_t array_handle = get_array_handle();
    std::stringstream array_handle_stream;
    array_handle_stream << std::hex << array_handle << std::dec;
    return std::move(array_handle_stream.str());
}

std::string MemoryDeviceEntry::get_error_handle_string() const
{
    uint16_t error_handle = get_error_handle();
    string error_handle_string = error_handle_map_[error_handle];
    if (!error_handle_string.empty()) {
        return error_handle_string;
    }
    std::stringstream err_handle_stream;
    err_handle_stream << std::hex << error_handle << std::dec;
    return std::move(err_handle_stream.str());
}

std::string MemoryDeviceEntry::get_total_width_string() const
{
    string width = data_width_map_[get_total_width()];
    if (!width.empty()) {
        return width;
    }
    width = std::to_string(get_total_width());
    width += " bits";
    return width;
}

std::string smbios::MemoryDeviceEntry::get_data_width_string() const
{
    string width = data_width_map_[get_data_width()];
    if (!width.empty()) {
        return width;
    }
    width = std::to_string(get_total_width());
    width += " bits";
    return width;
}

std::string smbios::MemoryDeviceEntry::get_device_size_string() const
{
    uint16_t device_size = get_device_size();
    string device_size_string = device_size_map_[device_size];
    if (!device_size_string.empty()) {
        return device_size_string;
    }

    if (device_size & 0x8000) {
        device_size_string += std::to_string(device_size & 0x7FFF);
        device_size_string += " kB";
    }
    else {
        device_size_string += std::to_string(device_size);
        device_size_string += " MB";
    }
    return device_size_string;
}

std::string MemoryDeviceEntry::get_form_factor_string() const
{
    string form_factor = form_factor_map_[get_form_factor()];
    assert(!form_factor.empty());
    return form_factor;
}

std::string MemoryDeviceEntry::get_device_set_string() const
{
    string device_set = device_set_map_[get_device_set()];
    if (!device_set.empty()) {
        return device_set;
    }
    return std::to_string(get_device_set());
}

std::string MemoryDeviceEntry::get_device_locator_string() const
{
    return "Not Specified";
}

std::string smbios::MemoryDeviceEntry::get_bank_locator_string() const
{
    return "Not Specified";
}

std::string smbios::MemoryDeviceEntry::get_device_type_string() const
{
    string device_type_string = device_type_map_[get_device_type()];
    assert(!device_type_string.empty());
    return device_type_string;
}

std::string smbios::MemoryDeviceEntry::get_device_detail_string() const
{
    std::stringstream properties_stream;
    constexpr size_t details_count = sizeof(uint16_t) * 8;
    uint16_t current_property = 0x1;
    const uint16_t properties = get_device_detail();
    for (size_t i = 0, current_property = 0x1; i < details_count; current_property <<= 1, ++i) {
        if (properties & current_property) {
            properties_stream << '\t' << device_properties_map_[current_property>>1] << '\n';
        }
    }
    return std::move(properties_stream.str());
}

void smbios::MemoryDeviceEntry::init_string_values()
{
    error_handle_map_[ErrorHandleNotProvided] = "Not Provided";
    error_handle_map_[ErrorHandleNoError] = "No Error";

    data_width_map_[DataWidthUnknown1] = "Unknown";
    data_width_map_[DataWidthUnknown2] = "Unknown";

    device_size_map_[DeviceSizeNoModuleInstalled] = "No Module Installed";
    device_size_map_[DeviceSizeUnknown] = "Unknown";

    form_factor_map_[FormFactorOutOfSpec] = "OutOfSpec";
    form_factor_map_[FormFactorOther] = "Other";
    form_factor_map_[FormFactorUnknown] = "Unknown";
    form_factor_map_[SIMM] = "SIMM";
    form_factor_map_[SIP] = "SIP";
    form_factor_map_[Chip] = "Chip";
    form_factor_map_[DIP] = "DIP";
    form_factor_map_[ZIP] = "ZIP";
    form_factor_map_[ProprietaryCard] = "ProprietaryCard";
    form_factor_map_[DIMM] = "DIMM";
    form_factor_map_[TSOP] = "TSOP";
    form_factor_map_[Rowofchips] = "Rowofchips";
    form_factor_map_[RIMM] = "RIMM";
    form_factor_map_[SODIMM] = "SODIMM";
    form_factor_map_[SRIMM] = "SRIMM";
    form_factor_map_[FBDIMM] = "FBDIMM";

    device_set_map_[DeviceSetNone] = "None";
    device_set_map_[DeviceSetUnknown] = "Unknown";

    device_type_map_[DeviceTypeOutOfSpec] = "OutOfSpec";
    device_type_map_[DeviceTypeOther] = "Other";
    device_type_map_[DeviceTypeUnknown] = "Unknown";
    device_type_map_[DRAM] = "DRAM";
    device_type_map_[EDRAM] = "EDRAM";
    device_type_map_[VRAM] = "VRAM";
    device_type_map_[SRAM] = "SRAM";
    device_type_map_[RAM] = "RAM";
    device_type_map_[ROM] = "ROM";
    device_type_map_[FLAS] = "FLAS";
    device_type_map_[EEPROM] = "EEPROM";
    device_type_map_[FEPROM] = "FEPROM";
    device_type_map_[EPROM] = "EPROM";
    device_type_map_[CDRAM] = "CDRAM";
    device_type_map_[D3DRAM] = "D3DRAM";
    device_type_map_[SDRAM] = "SDRAM";
    device_type_map_[SGRAM] = "SGRAM";
    device_type_map_[RDRAM] = "RDRAM";
    device_type_map_[DDR] = "DDR";
    device_type_map_[DDR2] = "DDR2";
    device_type_map_[DDR2FB] = "DDR2FB";
    device_type_map_[Reserved1] = "Reserved";
    device_type_map_[Reserved2] = "Reserved";
    device_type_map_[Reserved3] = "Reserved";
    device_type_map_[DDR3] = "DDR3";
    device_type_map_[FBD2] = "FBD2";
    device_type_map_[DDR4] = "DDR4";
    device_type_map_[LPDDR] = "LPDDR";
    device_type_map_[LPDDR2] = "LPDDR2";
    device_type_map_[LPDDR3] = "LPDDR3";
    device_type_map_[LPDDR4] = "LPDDR4";

    device_properties_map_[DevicePropertiesOutOfSpec] = "DevicePropertiesOutOfSpec";
    device_properties_map_[DevicePropertiesOther] = "DevicePropertiesOther";
    device_properties_map_[DevicePropertiesUnknown] = "DevicePropertiesUnknown";
    device_properties_map_[FastPaged] = "FastPaged";
    device_properties_map_[StaticColumn] = "StaticColumn";
    device_properties_map_[PseudoStatic] = "PseudoStatic";
    device_properties_map_[RAMBUS] = "RAMBUS";
    device_properties_map_[Synchronous] = "Synchronous";
    device_properties_map_[CMOS] = "CMOS";
    device_properties_map_[EDO] = "EDO";
    device_properties_map_[WindowDRAM] = "WindowDRAM";
    device_properties_map_[CacheDRAM] = "CacheDRAM";
    device_properties_map_[NonVolatile] = "NonVolatile";
    device_properties_map_[Registered] = "Registered";
    device_properties_map_[Unregistered] = "Unregistered";
    device_properties_map_[LRDIMM] = "LRDIMM";
}
