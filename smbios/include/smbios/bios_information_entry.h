#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

// BIOS Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'BIOS Information' chapter
// Major differences only between 2.4 and 3.1 versions

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

// should be aligned to be mapped to physical memory
#pragma pack(push, 1)

/// @brief SMBIOS BIOS Information entry Ver 2.4
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

/// @brief SMBIOS BIOS Information entry Ver 3.1
struct BiosInformationV31 : public BiosInformationV24 {
    uint16_t extended_rom_size;
};

#pragma pack(pop)

/// @brief  BIOS Information structure
class BiosInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief BIOS Characteristics bitwise layout
    // (*u suffix is obligatory for some compilers)
    enum BiosProperties : uint64_t {
        BiosPropertiesOutOfSpec = 0x0,
        Reserved1 = 0x1u << 0,
        Reserved2 = 0x1u << 1,
        Unknown = 0x1u << 2,
        NotSupported = 0x1u << 3,
        ISASupported = 0x1u << 4,
        MCASupported = 0x1u << 5,
        EISASupported = 0x1u << 6,
        PCISupported = 0x1u << 7,
        PCMCIASupported = 0x1u << 8,
        PnPSupported = 0x1u << 9,
        APMSupported = 0x1u << 10,
        BIOSUpgradeable = 0x1u << 11,
        BIOSShadowingAllowed = 0x1u << 12,
        VLVESASupported = 0x1u << 13,
        ESCDSupported = 0x1u << 14,
        BootFromCDSupported = 0x1u << 15,
        SelectableBootSupported = 0x1u << 16,
        BIOSROMSocketed = 0x1u << 17,
        BootFromPCMCIASupported = 0x1u << 18,
        EDDSpecificationSupported = 0x1u << 19,
        FloppyNECSupported = 0x1u << 20,
        FloppyToshibaSupported = 0x1u << 21,
        Floppy360kSupported = 0x1u << 22,
        Floppy12MSupported = 0x1u << 23,
        Floppy720kSupported = 0x1u << 24,
        Floppy28MSupported = 0x1u << 25,
        PrintScreenSupported = 0x1u << 26,
        KeyboardServicesSupported = 0x1u << 27,
        SerialServicesSupported = 0x1u << 28,
        PrinterServicesSupported = 0x1u << 29,
        MonoVideoSupported = 0x1u << 30,
        NECPC = 0x1u << 31
    };

    // @brief  BIOS Characteristics Extension Byte 1 layout
    // (*u suffix is obligatory for some compilers)
    enum BiosPropertiesEx1 : uint8_t {
        BiosPropertiesEx1OutOfSpec = 0x0,
        ACPISupported = 0x1u << 0,
        USBLegacySupported = 0x1u << 1,
        AGPSupported = 0x1u << 2,
        I2OBootSupported = 0x1u << 3,
        SuperDiskBootSupported = 0x1u << 4,
        ZIPDriveBootSupported = 0x1u << 5,
        IEEE1394BootSupported = 0x1u << 6,
        SmartBatterySupported = 0x1u << 7
    };

    // @brief  BIOS Characteristics Extension Byte 2 layout
    // (*u suffix is obligatory for some compilers)
    enum BiosPropertiesEx2 : uint8_t {
        BiosPropertiesEx2OutOfSpec = 0x0,
        BootSpecificationSupported = 0x1u << 0,
        KeyInitiatedNetworkBoot = 0x1u << 1,
        TargetedContentDistribution = 0x1u << 2,
        UEFISpecificationSupported = 0x1u << 3,
        VirtualMachine = 0x1u << 4
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in BIOS Information SMBIOS entry depending on version and size (should be compliant)
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
    /// Index of string number of the BIOS Vendor's Name
    uint8_t get_vendor_index() const;

    /// @brief 0x05 offset
    /// Index of string number of the BIOS Version, a free-form string
    uint8_t get_version_index() const;

    /// @brief 0x06 offset
    /// Segment location of BIOS starting address
    uint16_t get_starting_address() const;

    /// @brief 0x06 offset (yeah, same as starting offset)
    /// The size of the runtime BIOS image can be computed from offset 0x06 (see spec)
    uint32_t get_runtime_size() const;

    /// @brief 0x08 offset
    /// Index of release date string
    uint8_t get_release_date_index() const;

    /// @brief 0x09 offset
    /// size of the physical device containing the BIOS, in bytes
    /// FFh - size is 16MB or greater, see Extended BIOS ROM Size
    uint8_t get_rom_size() const;

    /// @brief 0x0A offset
    /// Defines which functions the BIOS supports: PCI, PCMCIA, Flash, etc
    uint64_t get_properties() const;

    /// @brief 0x12 offset
    /// BIOS Characteristics Extension Byte 1
    uint8_t get_properties_extension1() const;

    /// @brief 0x13 offset
    /// BIOS Characteristics Extension Byte 2
    uint8_t get_properties_extension2() const;

    /// @brief 0x14 offset
    /// Identifies the major release of the System BIOS
    uint8_t get_bios_major_release() const;

    /// @brief 0x15 offset
    /// Identifies the minor release of the System BIOS
    uint8_t get_bios_minor_release() const;

    /// @brief 0x16 offset
    /// Identifies the major release of the embedded controller firmware
    /// If the system does not have field upgradeable embedded controller firmware, 
    /// the value is 0xFF
    uint8_t get_firmware_major_release() const;

    /// @brief 0x17 offset
    /// Identifies the minor release of the embedded controller firmware
    /// If the system does not have field upgradeable embedded controller firmware, 
    /// the value is 0xFF
    uint8_t get_firmware_minor_release() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief BIOS vendor
    std::string get_vendor_string() const;

    /// @brief Version DMI string
    /// Free-form string that may contain
    /// Core and OEM version information
    std::string get_version_string() const;

    /// @brief Segment location of BIOS, HEX string
    std::string get_starting_address_string() const;

    /// @brief 
    /// The size of the runtime BIOS image, HEX string
    std::string get_runtime_size_string() const;

    /// @brief Index of release date DMI string
    /// String number of the BIOS release date
    /// is in either MM/DD/YY or MM/DD/YYYY format
    std::string get_release_date_string() const;

    /// @brief size of the physical device containing the BIOS
    /// Formatted with size (kb)
    std::string get_rom_size_string() const;

    /// @brief Return a \n\t-separated list on BIOS properties
    std::string get_properties_string() const;

    /// @brief Return a \n\t-separated list on BIOS extended byte 1 properties
    std::string get_properties_extension1_string() const;

    /// @brief Return a \n\t-separated list on BIOS extended byte 2 properties
    std::string get_properties_extension2_string() const;

    /// @brief BIOS Version in "major.minor" format
    std::string get_bios_version_string() const;

    /// @brief Firmware version in "major.minor" format
    std::string get_firmware_version_string() const;

private:

    /// Format version string
    std::string stream_to_version(uint16_t major, uint16_t minor) const;

    /// Map flags data to string values
    void init_string_values();

private:

    /// Initialization depends on SMBIOS version
    const BiosInformationV24* bios_information24_ = nullptr;
    const BiosInformationV31* bios_information31_ = nullptr;

    /// Bitwise to string representation
    std::map<uint64_t, std::string> properties_map_;
    std::map<uint8_t, std::string> properties_extensions1_map_;
    std::map<uint8_t, std::string> properties_extensions2_map_;
};

} // namespace smbios
