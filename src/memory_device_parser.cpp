#include <smbios_utility/smbios.h>
#include <smbios_utility/memory_device.h>
#include <smbios_utility/memory_device_parser.h>
#include <sstream>


MemoryDeviceParser::MemoryDeviceParser(const DMIHeader& header) : header_(header) {

    if (header.type != SMBios::MemoryDevice) {
        std::stringstream err;
        err << "Wrong device type, expected Memory device, called Type = " << header.type;
        throw std::runtime_error(err.str().c_str());
    }

    if (header.length >= 0x15) {
        memory_device_v1_ = reinterpret_cast<MemoryDeviceV1*>(header.data);
    }

    if (header.length >= 0x17) {
        memory_device_v2_ = reinterpret_cast<MemoryDeviceV2*>(header.data);
    }

    if (header.length >= 0x1B) {
        memory_device_v3_ = reinterpret_cast<MemoryDeviceV3*>(header.data);
    }

    if (header.length >= 0x1C) {
        memory_device_v4_ = reinterpret_cast<MemoryDeviceV4*>(header.data);
    }

}

uint16_t MemoryDeviceParser::get_error_handle() const
{
    if (memory_device_v1_) {
        return ErrorHandleValue::ErrorHandleNotProvided;
    }
    
    return memory_device_v1_->array_error_handle;
}

uint16_t MemoryDeviceParser::get_total_width() const
{
    if (nullptr == memory_device_v1_) {
        return DataWidthValue::DataWidthUnknown1;
    }

    return memory_device_v1_->total_width;
}

uint16_t MemoryDeviceParser::get_data_width() const
{
    if (nullptr == memory_device_v1_) {
        return DataWidthValue::DataWidthUnknown1;
    }

    return memory_device_v1_->data_width;
}

uint16_t MemoryDeviceParser::get_device_size() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceSizeValue::DeviceSizeUnknown;
    }

    return memory_device_v1_->device_size;
}

uint8_t MemoryDeviceParser::get_form_factor() const
{
    if (nullptr == memory_device_v1_) {
        return FormFactorValue::FormFactorOutOfSpec;
    }

    return memory_device_v1_->device_form_factor;
}

uint8_t MemoryDeviceParser::get_device_set() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceSetValue::DeviceSetUnknown;
    }

    return memory_device_v1_->device_set;
}

uint8_t MemoryDeviceParser::get_device_locator() const
{
    if (nullptr == memory_device_v1_) {
        return 0x0;
    }

    return memory_device_v1_->device_locator;
}

uint8_t MemoryDeviceParser::get_bank_locator() const
{
    if (nullptr == memory_device_v1_) {
        return 0x0;
    }

    return memory_device_v1_->bank_locator;
}

uint8_t MemoryDeviceParser::get_device_type() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceTypeValue::DeviceTypeOutOfSpec;
    }

    return memory_device_v1_->device_type;
}

uint16_t MemoryDeviceParser::get_device_detail() const
{
    if (nullptr == memory_device_v1_) {
        return DeviceProperties::DevicePropertiesOutOfSpec;
    }

    return memory_device_v1_->type_detail;
}
