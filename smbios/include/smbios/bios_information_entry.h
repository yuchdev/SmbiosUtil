#pragma once
#include <cstdint>
#include <map>
#include <smbios/abstract_smbios_entry.h>

namespace smbios {

struct DMIHeader;

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
    enum BiosProperties : uint32_t {
    };

    // @brief 
    enum BiosPropertiesEx1 : uint8_t {
    };

    // @brief 
    enum BiosPropertiesEx2 : uint8_t {
    };


    /// @brief Parse the header, recognize how much information do we have
    /// in MemoryDevice SMBIOS entry
    BiosInformationEntry(const DMIHeader& header);

    // @brief Parent is abstract
    virtual ~BiosInformationEntry();

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x06 offset
    /// 
    uint8_t* get_starting_address() const;

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

    /// @brief 
    const PortConnection* get_port_connection_struct() const { return port_connection_; }
    const BiosInformationV24* get_bios_information_struct_ver24 const { return bios_information24_; }
    const BiosInformationV31* get_bios_information_struct_ver31 const { return bios_information31_; }



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