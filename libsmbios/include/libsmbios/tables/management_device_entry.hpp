#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Management Device entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Management Device' chapter
// (7.35), DSP0134 3.9.0. Field offsets (Table 107), the Type field's
// enumerated values (7.35.1, Table 108) and the Address Type field's
// enumerated values (7.35.2, Table 109) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Management Device structure (7.35) - a hardware monitoring
/// device (e.g. an LM75-family temperature/voltage sensor chip) that might
/// control one or more fans or voltage/current/temperature probes, each
/// described by its own Management Device Component structure (7.36, see
/// ManagementDeviceComponentEntry) referencing this structure's own handle.
class ManagementDeviceEntry : public AbstractSMBiosEntry {
public:

    /// @brief Device's Type field (0x05), DSP0134 3.9.0 Table 108. Named
    /// `DeviceType` (accessors `get_device_type()`/`get_device_type_string()`)
    /// rather than `Type`/`get_type()` to avoid colliding with
    /// `AbstractSMBiosEntry::get_type()` (the structure-kind string), the
    /// same hazard as Story 04.0's `PointingDeviceEntry`.
    /// `DeviceTypeOutOfSpec` (value 0) is not a spec value; this project's
    /// own fallback sentinel for "field absent or an unassigned byte value",
    /// matching this codebase's established `*OutOfSpec` precedent.
    enum DeviceType : uint8_t {
        DeviceTypeOutOfSpec = 0x00,
        DeviceTypeOther = 0x01,
        DeviceTypeUnknown = 0x02,
        DeviceTypeNationalSemiconductorLm75 = 0x03,
        DeviceTypeNationalSemiconductorLm78 = 0x04,
        DeviceTypeNationalSemiconductorLm79 = 0x05,
        DeviceTypeNationalSemiconductorLm80 = 0x06,
        DeviceTypeNationalSemiconductorLm81 = 0x07,
        DeviceTypeAnalogDevicesAdm9240 = 0x08,
        DeviceTypeDallasSemiconductorDs1780 = 0x09,
        DeviceTypeMaxim1617 = 0x0A,
        DeviceTypeGenesysGl518sm = 0x0B,
        DeviceTypeWinbondW83781d = 0x0C,
        DeviceTypeHoltekHt82h791 = 0x0D
    };

    /// @brief Address Type field (0x0A), DSP0134 3.9.0 Table 109 (7.35.2).
    /// `AddressTypeOutOfSpec` (value 0) is not a spec value; this project's
    /// own fallback sentinel, matching the same convention as `DeviceType`.
    enum AddressType : uint8_t {
        AddressTypeOutOfSpec = 0x00,
        AddressTypeOther = 0x01,
        AddressTypeUnknown = 0x02,
        AddressTypeIoPort = 0x03,
        AddressTypeMemory = 0x04,
        AddressTypeSmBus = 0x05
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ManagementDeviceEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ManagementDeviceEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset DMI string. Additional descriptive information
    /// about the device or its location. Returns an empty string if
    /// get_entry_size() < 0x05.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Byte/dword values

    /// @brief 0x05 offset, raw byte. See DeviceType for values. Returns
    /// DeviceTypeOutOfSpec if get_entry_size() < 0x06.
    DeviceType get_device_type() const;

    /// @brief String representation of get_device_type().
    std::string get_device_type_string() const;

    /// @brief 0x06 offset, 4 raw bytes. Device's address; interpretation
    /// depends on get_address_type(). Returns 0 if get_entry_size() < 0x0A.
    uint32_t get_address() const;

    /// @brief 0x0A offset, raw byte. See AddressType for values. Returns
    /// AddressTypeOutOfSpec if get_entry_size() < 0x0B.
    AddressType get_address_type() const;

    /// @brief String representation of get_address_type().
    std::string get_address_type_string() const;

private:

    /// Map DeviceType/AddressType values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> device_type_map_;
    std::map<uint8_t, std::string> address_type_map_;
};

} // namespace smbios
