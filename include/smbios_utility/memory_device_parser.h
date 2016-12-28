#pragma once

struct DMIHeader;

class MemoryDeviceParser {
public:

    enum ErrorHandleValue : uint16_t {
        ErrorHandleNotProvided = 0xFFFE,
        ErrorHandleNoError = 0xFFFF
    };

    enum DataWidthValue : uint16_t {
        DataWidthUnknown1 = 0x0,
        DataWidthUnknown2 = 0xFFFF
    };

    enum DeviceSizeValue : uint16_t
    {
        DeviceSizeNoModuleInstalled = 0x0,
        DeviceSizeUnknown = 0xFFFF
    };

    enum FormFactorValue : uint8_t {
        FormFactorOutOfSpec = 0x00,
        FormFactorOther = 0x01,
        FormFactorUnknown = 0x02,
        SIMM = 0x03, 
        SIP = 0x04, 
        Chip = 0x05, 
        DIP = 0x06, 
        ZIP = 0x07, 
        ProprietaryCard = 0x08, 
        DIMM = 0x09, 
        TSOP = 0x0A, 
        Rowofchips = 0x0B, 
        RIMM = 0x0C, 
        SODIMM = 0x0D, 
        SRIMM = 0x0E, 
        FBDIMM = 0x0F,
    };

    enum DeviceSetValue : uint8_t
    {
        DeviceSetNone = 0x0,
        DeviceSetUnknown = 0xFF
    };

    enum DeviceTypeValue : uint8_t
    {
        DeviceTypeOutOfSpec = 0x00,
        DeviceTypeOther = 0x01,
        DeviceTypeUnknown = 0x02,
        DRAM = 0x03,
        EDRAM = 0x04,
        VRAM = 0x05,
        SRAM = 0x06,
        RAM = 0x07,
        ROM = 0x08,
        FLAS = 0x09,
        EEPROM = 0x0A,
        FEPROM = 0x0B,
        EPROM = 0x0C,
        CDRAM = 0x0D,
        D3DRAM = 0x0E,
        SDRAM = 0x0F,
        SGRAM = 0x10,
        RDRAM = 0x11,
        DDR = 0x12,
        DDR2 = 0x13,
        DDR2FB = 0x14,
        Reserved1 = 0x15,
        Reserved2 = 0x16,
        Reserved3 = 0x17,
        DDR3 = 0x18,
        FBD2 = 0x19,
        DDR4 = 0x1A,
        LPDDR = 0x1B,
        LPDDR2 = 0x1C,
        LPDDR3 = 0x1D,
        LPDDR4 = 0x1E,
    };

    enum DeviceProperties : uint16_t
    {
        DevicePropertiesOutOfSpec = 0,
        DevicePropertiesOther = 0x1,
        DevicePropertiesUnknown = 0x1 << 1,
        FastPaged = 0x1 << 2,
        StaticColumn = 0x1 << 3,
        PseudoStatic = 0x1 << 4,
        RAMBUS = 0x1 << 5,
        Synchronous = 0x1 << 6,
        CMOS = 0x1 << 7,
        EDO = 0x1 << 8,
        WindowDRAM = 0x1 << 9,
        CacheDRAM = 0x1 << 10,
        NonVolatile = 0x1 << 11,
        Registered = 0x1 << 12,
        Unregistered = 0x1 << 13,
        LRDIMM = 0x1 << 14
    };

    MemoryDeviceParser(const DMIHeader& header);

    const char* get_type() {
        return "Memory Device";
    }

    const MemoryDeviceV1* get_memory_device_v1() const { return memory_device_v1_; }
    const MemoryDeviceV2* get_memory_device_v2() const { return memory_device_v2_; }
    const MemoryDeviceV3* get_memory_device_v3() const { return memory_device_v3_; }
    const MemoryDeviceV4* get_memory_device_v4() const { return memory_device_v4_; }

    uint16_t get_error_handle() const;

    uint16_t get_total_width() const;

    uint16_t get_data_width() const;

    uint16_t get_device_size() const;

    uint8_t get_form_factor() const;

    uint8_t get_device_set() const;

    uint8_t get_device_locator() const;

    uint8_t get_bank_locator() const;

    uint8_t get_device_type() const;

    uint16_t get_device_detail() const;


private:
    const DMIHeader& header_;
    const MemoryDeviceV1* memory_device_v1_ = nullptr;
    const MemoryDeviceV2* memory_device_v2_ = nullptr;
    const MemoryDeviceV3* memory_device_v3_ = nullptr;
    const MemoryDeviceV4* memory_device_v4_ = nullptr;
};