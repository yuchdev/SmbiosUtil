#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// System Enclosure (a.k.a. Chassis Information) entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Enclosure or Chassis'
// chapter (7.4). Cross-checked against c-versions/dmidecode_linux/dmidecode.c's
// case 3 (dmi_decode(), ~line 4536, titled "Chassis Information" there - same
// structure type SMBios::SMBiosHandler calls SystemEnclosure) and its
// dmi_chassis_type()/dmi_chassis_lock()/dmi_chassis_state()/
// dmi_chassis_security_status() helpers for field offsets, length guards and
// enum value tables.

namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The System Enclosure (chassis) structure - manufacturer/type/
/// version/serial/asset tag, boot-up/power/thermal/security state, OEM-
/// defined data, height/power cords/contained elements (2.1+), and SKU
/// number (2.7+, at a runtime-computed offset past the contained-elements
/// array).
///
/// Scope note on the contained-elements array: only the raw count
/// (get_contained_element_count()) and per-record length
/// (get_contained_element_record_length()) are exposed. Decoding each
/// contained element's own (type, minimum, maximum) triple - a nested,
/// variable-length sub-structure not otherwise named by this class's own
/// task brief - is out of scope here; the count/record-length pair is
/// exactly what the SKU number field's own runtime offset computation
/// needs, which is why those two accessors exist.
class SystemEnclosureEntry : public AbstractSMBiosEntry {
public:

    // @brief Chassis Type field (offset 0x05, bits 0-6), see
    // dmi_chassis_type() in the reference (7.4.1 of the spec). Bit 7 of the
    // same byte is the separate "lock present" flag - see get_lock_present().
    enum ChassisType : uint8_t {
        ChassisTypeOutOfSpec = 0x00,
        ChassisTypeOther = 0x01,
        ChassisTypeUnknown = 0x02,
        Desktop = 0x03,
        LowProfileDesktop = 0x04,
        PizzaBox = 0x05,
        MiniTower = 0x06,
        Tower = 0x07,
        Portable = 0x08,
        Laptop = 0x09,
        Notebook = 0x0A,
        HandHeld = 0x0B,
        DockingStation = 0x0C,
        AllInOne = 0x0D,
        SubNotebook = 0x0E,
        SpaceSaving = 0x0F,
        LunchBox = 0x10,
        MainServerChassis = 0x11,
        ExpansionChassis = 0x12,
        SubChassis = 0x13,
        BusExpansionChassis = 0x14,
        PeripheralChassis = 0x15,
        RAIDChassis = 0x16,
        RackMountChassis = 0x17,
        SealedCasePC = 0x18,
        MultiSystem = 0x19,
        CompactPCI = 0x1A,
        AdvancedTCA = 0x1B,
        Blade = 0x1C,
        BladeEnclosing = 0x1D,
        Tablet = 0x1E,
        Convertible = 0x1F,
        Detachable = 0x20,
        IoTGateway = 0x21,
        EmbeddedPC = 0x22,
        MiniPC = 0x23,
        StickPC = 0x24
    };

    // @brief Boot-up State / Power Supply State / Thermal State field value
    // table, see dmi_chassis_state() in the reference (7.4.2 of the spec).
    // All three fields share this same enum/value table.
    enum ChassisState : uint8_t {
        ChassisStateOutOfSpec = 0x00,
        ChassisStateOther = 0x01,
        ChassisStateUnknown = 0x02,
        Safe = 0x03,
        Warning = 0x04,
        Critical = 0x05,
        NonRecoverable = 0x06
    };

    // @brief Security Status field, see dmi_chassis_security_status() in the
    // reference (7.4.3 of the spec).
    enum SecurityStatus : uint8_t {
        SecurityStatusOutOfSpec = 0x00,
        SecurityStatusOther = 0x01,
        SecurityStatusUnknown = 0x02,
        None = 0x03,
        ExternalInterfaceLockedOut = 0x04,
        ExternalInterfaceEnabled = 0x05
    };

    /// @brief Parse the header, recognize how much information do we have
    /// in System Enclosure SMBIOS entry depending on version and size
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemEnclosureEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemEnclosureEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order;
    /// "Lock" is a bool (chassis lock present).
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte values

    /// @brief 0x04 offset
    /// Index of string number of the chassis manufacturer
    uint8_t get_manufacturer_index() const;

    /// @brief 0x05 offset, bits 0-6
    /// See ChassisType enum for values
    uint8_t get_enclosure_type() const;

    /// @brief 0x05 offset, bit 7
    /// Whether the chassis has a functioning lock
    bool get_lock_present() const;

    /// @brief 0x06 offset
    /// Index of string number of the version
    uint8_t get_version_index() const;

    /// @brief 0x07 offset
    /// Index of string number of the serial number
    uint8_t get_serial_number_index() const;

    /// @brief 0x08 offset
    /// Index of string number of the asset tag
    uint8_t get_asset_tag_index() const;

    /// @brief 0x09 offset
    /// See ChassisState enum for values
    uint8_t get_boot_up_state() const;

    /// @brief 0x0A offset
    /// See ChassisState enum for values
    uint8_t get_power_supply_state() const;

    /// @brief 0x0B offset
    /// See ChassisState enum for values
    uint8_t get_thermal_state() const;

    /// @brief 0x0C offset
    /// See SecurityStatus enum for values
    uint8_t get_security_status() const;

    /// @brief 0x0D offset
    /// OEM- or BIOS vendor-specific information
    uint32_t get_oem_defined() const;

    /// @brief 0x11 offset (2.1+)
    /// Height of the enclosure, in U (rack units); 0 means unspecified
    uint8_t get_height() const;

    /// @brief 0x12 offset (2.1+)
    /// Number of power cords associated with the enclosure; 0 means
    /// unspecified
    uint8_t get_number_of_power_cords() const;

    /// @brief 0x13 offset (2.1+)
    /// Number of Contained Element records that follow, at 0x15
    uint8_t get_contained_element_count() const;

    /// @brief 0x14 offset (2.1+)
    /// Byte length of each Contained Element record at 0x15; 0 if
    /// get_contained_element_count() is 0
    uint8_t get_contained_element_record_length() const;

    /// @brief Runtime-computed offset `0x15 + get_contained_element_count() *
    /// get_contained_element_record_length()` (2.7+): there is no fixed
    /// offset for this field, since it follows the variable-length
    /// contained-elements array. See the .cpp implementation for the
    /// computed-offset-plus-length-guard pattern.
    /// Index of string number of the SKU number
    uint8_t get_sku_number_index() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Chassis manufacturer
    std::string get_manufacturer_string() const;

    /// @brief Enclosure type string representation
    std::string get_enclosure_type_string() const;

    /// @brief Lock present string representation ("Present"/"Not Present")
    std::string get_lock_present_string() const;

    /// @brief Version DMI string
    std::string get_version_string() const;

    /// @brief Serial number DMI string
    std::string get_serial_number_string() const;

    /// @brief Asset tag DMI string
    std::string get_asset_tag_string() const;

    /// @brief Boot-up state string representation
    std::string get_boot_up_state_string() const;

    /// @brief Power supply state string representation
    std::string get_power_supply_state_string() const;

    /// @brief Thermal state string representation
    std::string get_thermal_state_string() const;

    /// @brief Security status string representation
    std::string get_security_status_string() const;

    /// @brief OEM-defined data, HEX string
    std::string get_oem_defined_string() const;

    /// @brief Height string representation ("Unspecified" when 0)
    std::string get_height_string() const;

    /// @brief Number of power cords string representation ("Unspecified"
    /// when 0)
    std::string get_number_of_power_cords_string() const;

    /// @brief SKU number DMI string (2.7+)
    std::string get_sku_number_string() const;

private:

    /// Map flags data to string values
    void init_string_values();

    /// String lookup shared by boot_up_state/power_supply_state/thermal_state
    std::string chassis_state_string(uint8_t state) const;

private:

    /// Bitwise to string representation
    std::map<uint8_t, std::string> chassis_type_map_;
    std::map<uint8_t, std::string> chassis_state_map_;
    std::map<uint8_t, std::string> security_status_map_;
};

} // namespace smbios
