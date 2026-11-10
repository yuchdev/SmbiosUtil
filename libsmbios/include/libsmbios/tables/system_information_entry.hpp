#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios.hpp>

// System Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Information' chapter (7.2)
// Cross-checked against c-versions/dmidecode_linux/dmidecode.c's case 1
// (dmi_decode(), ~line 4484) and its dmi_system_uuid()/dmi_system_wake_up_type()
// helpers for field offsets, length guards and enum value tables.

namespace smbios {

struct DMIHeader;
// SMBiosVersion (smbios.hpp) is included for real, not just forward-declared:
// this class stores a copy of it (version_) for the UUID's version-gated
// byte-swap, so its full definition is needed here.

/// @brief The System Information structure - manufacturer/product/version/
/// serial number, a 16-byte UUID, wake-up type, and (2.4+) SKU number/family.
class SystemInformationEntry : public AbstractSMBiosEntry {
public:

    // @brief Wake-up Type field (offset 0x18), see dmi_system_wake_up_type()
    // in the reference (7.2.2 of the spec)
    enum WakeUpType : uint8_t {
        Reserved = 0x00,
        Other = 0x01,
        Unknown = 0x02,
        APMTimer = 0x03,
        ModemRing = 0x04,
        LANRemote = 0x05,
        PowerSwitch = 0x06,
        PCIPME = 0x07,
        ACPowerRestored = 0x08
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in System Information SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemInformationEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Index of string number of the system manufacturer
    uint8_t get_manufacturer_index() const;

    /// @brief 0x05 offset
    /// Index of string number of the product name
    uint8_t get_product_name_index() const;

    /// @brief 0x06 offset
    /// Index of string number of the version
    uint8_t get_version_index() const;

    /// @brief 0x07 offset
    /// Index of string number of the serial number
    uint8_t get_serial_number_index() const;

    /// @brief 0x18 offset
    /// Identifies the event that caused the system to power up
    /// See WakeUpType enum for values
    uint8_t get_wake_up_type() const;

    /// @brief 0x19 offset (2.4+)
    /// Index of string number of the SKU number
    uint8_t get_sku_number_index() const;

    /// @brief 0x1A offset (2.4+)
    /// Index of string number of the family
    uint8_t get_family_index() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief System manufacturer
    std::string get_manufacturer_string() const;

    /// @brief Product name DMI string
    std::string get_product_name_string() const;

    /// @brief Version DMI string
    std::string get_version_string() const;

    /// @brief Serial number DMI string
    std::string get_serial_number_string() const;

    /// @brief 0x08 offset, 16 raw bytes formatted as a hyphenated hex UUID.
    /// Special sentinel values (all 0xFF -> "Not Present", all 0x00 ->
    /// "Not Settable") are checked first. Otherwise, as of SMBIOS 2.6 the
    /// first 3 fields are little-endian multi-byte integers that must be
    /// byte-swapped to print in conventional big-endian hex order; versions
    /// before 2.6 print the 16 bytes straight through, unswapped. See
    /// dmi_system_uuid() in the reference for the exact byte order this
    /// reproduces.
    std::string get_uuid_string() const;

    /// @brief Wake-up Type string representation
    std::string get_wake_up_type_string() const;

    /// @brief SKU number DMI string (2.4+)
    std::string get_sku_number_string() const;

    /// @brief Family DMI string (2.4+)
    std::string get_family_string() const;

private:

    /// Format the 16 raw UUID bytes at `uuid` per the SMBIOS version's byte
    /// order convention (see get_uuid_string()'s doc comment).
    static std::string format_uuid(const uint8_t* uuid, const SMBiosVersion& version);

    /// Map flags data to string values
    void init_string_values();

private:

    /// Copy of the SMBIOS version, needed only for the UUID's version-gated
    /// byte-swap (every other field in this structure is distinguished by
    /// header.length alone).
    SMBiosVersion version_;

    /// Bitwise to string representation
    std::map<uint8_t, std::string> wake_up_type_map_;
};

} // namespace smbios
