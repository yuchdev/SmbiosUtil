#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

// should be aligned to be mapped to physical memory
#pragma pack(push, 1)


// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version
// 'Memory Device' chapter

/// @brief SMBIOS MemoryDevice entry Ver 2.1+
struct MemoryDeviceV21 {
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
struct MemoryDeviceV23 : public MemoryDeviceV21 {
    uint16_t device_speed;
};

/// @brief SMBIOS MemoryDevice entry Ver 2.6+
struct MemoryDeviceV26 : public MemoryDeviceV23 {
    uint8_t manufacturer;
    uint8_t serial_number;
    uint8_t asset_tag;
    uint8_t part_number;
};

/// @brief SMBIOS MemoryDevice entry Ver 2.7+
struct MemoryDeviceV27 : public MemoryDeviceV26 {
    uint8_t device_rank;
    uint32_t extended_size;
    uint16_t memory_clock_speed;
};

// Further versions contain more information like voltage, but do not parsed here

#pragma pack(pop)

/// @brief Class-wrapper under raw memory structures
/// Class instance does not "own" this memory, it just provide more convenient interface
/// and useful types and enumerations
class MemoryDeviceEntry : public AbstractSMBiosEntry {
public:

    // @brief special values for ErrorHandle: uint16 - offset 0x06
    enum ErrorHandleValue : uint16_t {
        ErrorHandleNotProvided = 0xFFFE,
        ErrorHandleNoError = 0xFFFF
    };

    // @brief special values for TotalWidth & DataWidth uint16 - offset 0x08, 0x0A
    enum DataWidthValue : uint16_t {
        DataWidthUnknown1 = 0x0,
        DataWidthUnknown2 = 0xFFFF
    };

    // @brief special values for DeviceSize: uint16 - offset 0x0C
    enum DeviceSizeValue : uint16_t
    {
        DeviceSizeNoModuleInstalled = 0x0,
        DeviceSizeUnknown = 0xFFFF
    };

    // @brief special values for FormFactor: uint8 - offset 0x0E
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

    // @brief special values for DeviceSet: uint8 - offset 0x0F
    enum DeviceSetValue : uint8_t
    {
        DeviceSetNone = 0x0,
        DeviceSetUnknown = 0xFF
    };

    // @brief special values for DeviceType: uint8 - offset 0x12
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

    // @brief Bit-mask values for DeviceProperties: uint16 - offset 0x13
    enum DeviceProperties : uint16_t
    {
        DevicePropertiesOutOfSpec = 0x0,
        DevicePropertiesReserved = 0x1 << 0,
        DevicePropertiesOther = 0x1 << 1,
        DevicePropertiesUnknown = 0x1 << 2,
        FastPaged = 0x1 << 3,
        StaticColumn = 0x1 << 4,
        PseudoStatic = 0x1 << 5,
        RAMBUS = 0x1 << 6,
        Synchronous = 0x1 << 7,
        CMOS = 0x1 << 8,
        EDO = 0x1 << 9,
        WindowDRAM = 0x1 << 10,
        CacheDRAM = 0x1 << 11,
        NonVolatile = 0x1 << 12,
        Registered = 0x1 << 13,
        Unregistered = 0x1 << 14,
        LRDIMM = 0x1 << 15
    };

    enum DeviceSpeed : uint16_t {
        DeviceSpeedUnknown = 0x0,
        DeviceSpeedReserved = 0xFFFF
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in MemoryDevice SMBIOS entry
    MemoryDeviceEntry(const DMIHeader& header, const SMBiosVersion& version);

    // @brief Parent is abstract
    virtual ~MemoryDeviceEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Bitwise values

    /// @brief 0x05 offset
    /// Handle, or instance number, associated with the structure
    uint16_t get_array_handle() const;

    /// @brief 0x06 offset
    /// See ErrorHandleValue enum for special values
    uint16_t get_error_handle() const;

    /// @brief 0x08 offset
    /// See DataWidthValue enum for special values
    uint16_t get_total_width() const;

    /// @brief 0x0A offset
    /// See DataWidthValue enum for special values
    uint16_t get_data_width() const;

    /// @brief 0x0C offset
    /// See DeviceSizeValue enum for special values
    uint16_t get_device_size() const;

    /// @brief 0x0E offset
    /// See FormFactorValue enum for special values
    uint8_t get_form_factor() const;

    /// @brief 0x0F offset
    /// See DeviceSetValue enum for special values
    uint8_t get_device_set() const;

    /// @brief 0x10 offset
    uint8_t get_device_locator() const;

    /// @brief 0x11 offset
    uint8_t get_bank_locator() const;

    /// @brief 0x12 offset
    /// See DeviceTypeValue enum for special values
    uint8_t get_device_type() const;

    /// @brief 0x13 offset
    /// See DeviceProperties enum for special values
    uint16_t get_device_detail() const;

    /// @brief 0x15 offset
    /// Identifies the maximum capable speed of the device
    uint16_t get_device_speed() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x05 offset
    /// Handle string representation
    std::string get_array_handle_string() const;

    /// @brief 0x06 offset
    /// ErrorHandleValue string representation
    std::string get_error_handle_string() const;

    /// @brief 0x08 offset
    /// DataWidthValue string representation
    std::string get_total_width_string() const;

    /// @brief 0x0A offset
    /// DataWidthValue string representation
    std::string get_data_width_string() const;

    /// @brief 0x0C offset
    /// DeviceSizeValue string representation
    std::string get_device_size_string() const;

    /// @brief 0x0E offset
    /// FormFactorValue string representation
    std::string get_form_factor_string() const;

    /// @brief 0x0F offset
    /// DeviceSetValue string representation
    std::string get_device_set_string() const;

    /// @brief 0x10 offset
    /// DeviceLocator string representation
    std::string get_device_locator_string() const;

    /// @brief 0x11 offset
    /// BankLocator string representation
    std::string get_bank_locator_string() const;

    /// @brief 0x12 offset
    /// DeviceType string representation
    std::string get_device_type_string() const;

    /// @brief 0x13 offset
    /// DeviceProperties string representation
    std::string get_device_detail_string() const;

    /// @brief 0x15 offset
    /// Maximum capable speed string representation
    std::string get_device_speed_string() const;

private:

    /// Map flags data to string values
    void init_string_values();

private:

    /// Init pointers depend on SMBIOS version
    const MemoryDeviceV21* memory_device_v1_ = nullptr;
    const MemoryDeviceV23* memory_device_v2_ = nullptr;
    const MemoryDeviceV26* memory_device_v3_ = nullptr;
    const MemoryDeviceV27* memory_device_v4_ = nullptr;

    /// Bitwise to string representation
    std::map<uint16_t, std::string> error_handle_map_;
    std::map<uint16_t, std::string> data_width_map_;
    std::map<uint16_t, std::string> device_size_map_;
    std::map<uint8_t, std::string> form_factor_map_;
    std::map<uint8_t, std::string> device_set_map_;
    std::map<uint8_t, std::string> device_type_map_;
    std::map<uint16_t, std::string> device_properties_map_;
    std::map<uint16_t, std::string> device_speed_map_;
};

} // namespace smbios