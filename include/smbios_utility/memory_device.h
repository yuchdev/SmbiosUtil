#pragma once
#include <cstdint>

// should be aligned to be mapped to physical memory
#pragma pack(push, 1)


// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version
// 'Memory Device' chapter

/// @brief SMBIOS MemoryDevice entry Ver 2.1+
struct MemoryDeviceV1 {
    uint32_t header;
    uint16_t array_handle;
    uint16_t array_error_handle;
    uint16_t total_width;
    uint16_t data_width;
    uint16_t device_size;
    uint8_t device_form_factor;
    uint8_t device_set;
    uint8_t device_locator;
    uint8_t bank_locator;
    uint8_t device_type;
    uint16_t type_detail;
};

/// @brief SMBIOS MemoryDevice entry Ver 2.3+
struct MemoryDeviceV2 : public MemoryDeviceV1 {
    uint16_t device_speed;
};

/// @brief SMBIOS MemoryDevice entry Ver 2.6+
struct MemoryDeviceV3 : public MemoryDeviceV2 {
    uint8_t manufacturer;
    uint8_t serial_number;
    uint8_t asset_tag;
    uint8_t part_number;
};

/// @brief SMBIOS MemoryDevice entry Ver 2.7+
struct MemoryDeviceV4 : public MemoryDeviceV3 {
    uint8_t device_rank;
    uint32_t extended_size;
    uint16_t memory_clock_speed;
};

// Further versions contain more information like voltage, but do not parsed here

#pragma pack(pop)
