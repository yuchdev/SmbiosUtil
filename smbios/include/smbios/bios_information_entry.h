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
// Standard according to current SMBIOS version, 'BIOS Information' chapter
// Major differences only between 2.4 and 3.1 versions

/// @brief SMBIOS MemoryDevice entry Ver 2.4
struct BiosInformationV24 {
    uint32_t header;
    uint8_t vendor;
    uint8_t bios_version;
    uint16_t starting_segment;
    uint8_t release_date;
    uint8_t rom_size;
    uint64_t bios_properties;
    uint8_t properties_extension1;
    uint8_t properties_extension2;
    uint8_t bios_major_version;
    uint8_t bios_minor_version;
    uint8_t firmware_major_version;
    uint8_t firmware_minor_version;
};

struct BiosInformationV31 : public BiosInformationV24 {
    uint16_t extended_rom_size;
};

#pragma pack(pop)

/// @brief 
class BiosInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief 
    enum BiosProperties : uint64_t {
        BiosPropertiesOutOfSpec = 0x0,
        Reserved1 = 0x1 << 0,
        Reserved2 = 0x1 << 1,
        Unknown = 0x1 << 2,
        NotSupported = 0x1 << 3,
        ISASupported = 0x1 << 4,
        MCASupported = 0x1 << 5,
        EISASupported = 0x1 << 6,
        PCISupported = 0x1 << 7,
        PCMCIASupported = 0x1 << 8,
        PnPSupported = 0x1 << 9,
        APMSupported = 0x1 << 10,
        BIOSUpgradeable = 0x1 << 11,
        BIOSShadowingAllowed = 0x1 << 12,
        VLVESASupported = 0x1 << 13,
        ESCDSupported = 0x1 << 14,
        BootFromCDSupported = 0x1 << 15,
        SelectableBootSupported = 0x1 << 16,
        BIOSROMSocketed = 0x1 << 17,
        BootFromPCMCIASupported = 0x1 << 18,
        EDDSpecificationSupported = 0x1 << 19,
        FloppyNECSupported = 0x1 << 20,
        FloppyToshibaSupported = 0x1 << 21,
        Floppy360kSupported = 0x1 << 22,
        Floppy12MSupported = 0x1 << 23,
        Floppy720kSupported = 0x1 << 24,
        Floppy28MSupported = 0x1 << 25,
        PrintScreenSupported = 0x1 << 26,
        KeyboardServicesSupported = 0x1 << 27,
        SerialServicesSupported = 0x1 << 28,
        PrinterServicesSupported = 0x1 << 29,
        MonoVideoSupported = 0x1 << 30,
        NECPC = 0x1 << 31
    };

    // @brief 
    enum BiosPropertiesEx1 : uint8_t {
        BiosPropertiesEx1OutOfSpec = 0x0,
        ACPISupported = 0x1 << 0,
        USBLegacySupported = 0x1 << 1,
        AGPSupported = 0x1 << 2,
        I2OBootSupported = 0x1 << 3,
        SuperDiskBootSupported = 0x1 << 4,
        ZIPDriveBootSupported = 0x1 << 5,
        IEEE1394BootSupported = 0x1 << 6,
        SmartBatterySupported = 0x1 << 7
    };

    // @brief 
    enum BiosPropertiesEx2 : uint8_t {
        BiosPropertiesEx2OutOfSpec = 0x0,
        BootSpecificationSupported = 0x1 << 0,
        KeyInitiatedNetworkBoot = 0x1 << 1,
        TargetedContentDistribution = 0x1 << 2,
        UEFISpecificationSupported = 0x1 << 3,
        VirtualMachine = 0x1 << 4
    };


    /// @brief Parse the header, recognize how much information do we have
    /// in MemoryDevice SMBIOS entry
    BiosInformationEntry(const DMIHeader& header, const SMBiosVersion& version);

    // @brief Parent is abstract
    virtual ~BiosInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Index of vendor DMI string
    uint8_t get_vendor_index() const;

    /// @brief 0x04 offset
    /// Index of version DMI string
    uint8_t get_version_index() const;

    /// @brief 0x06 offset
    /// 
    uint16_t get_starting_address() const;

    /// @brief 0x08 offset
    /// Index of release date DMI string
    uint8_t get_release_date_index() const;

    /// @brief 0x09 offset
    /// 
    uint8_t get_rom_size() const;

    /// @brief 0x0A offset
    /// 
    uint64_t get_properties() const;

    /// @brief 0x12 offset
    /// 
    uint8_t get_properties_extension1() const;

    /// @brief 0x13 offset
    /// 
    uint8_t get_properties_extension2() const;

    /// @brief 0x14 offset
    /// 
    uint8_t get_bios_major_release() const;

    /// @brief 0x15 offset
    /// 
    uint8_t get_bios_minor_release() const;

    /// @brief 0x16 offset
    /// 
    uint8_t get_firmware_major_release() const;

    /// @brief 0x17 offset
    /// 
    uint8_t get_firmware_minor_release() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Vendor DMI string
    std::string get_vendor_string() const;

    /// @brief Version DMI string
    std::string get_version_string() const;

    /// @brief 
    std::string get_starting_address_string() const;

    /// @brief Index of release date DMI string
    std::string get_release_date_string() const;

    /// @brief 
    std::string get_rom_size_string() const;

    /// @brief 
    std::string get_properties_string() const;

    /// @brief 
    std::string get_properties_extension1_string() const;

    /// @brief 
    std::string get_properties_extension2_string() const;

    /// @brief 
    std::string get_bios_version_string() const;

    /// @brief 
    std::string get_firmware_version_string() const;

private:

    /// Map flags data to string values
    void init_string_values();

private:

    const BiosInformationV24* bios_information24_ = nullptr;
    const BiosInformationV31* bios_information31_ = nullptr;

    /// Bitwise to string representation
    std::map<uint64_t, std::string> properties_map_;
    std::map<uint8_t, std::string> properties_extensions1_map_;
    std::map<uint8_t, std::string> properties_extensions2_map_;
};

} // namespace smbios