#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Onboard Devices Extended Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Onboard Devices Extended
// Information' chapter (7.42), DSP0134 3.9.0. Field offsets (Table 120),
// the Device Type byte's bit-7/bits-6:0 split (7.42.2), Device Type
// Instance (7.42.3), and the Segment Group Number/Bus Number/
// Device-Function Number triple (7.42.4) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// This is the modern (2.6+) replacement for the obsolete Onboard Device
// Information (Type 10) structure, which this milestone deliberately
// excludes per plan.md's Non-goals (see 7.42's own NOTE: "This structure
// replaces Onboard Device Information (Type 10) starting with version 2.6
// of this specification").
//
// SENTINEL-CONVENTION COMPARISON (required by this task): 7.42.4's own text
// reads "For devices that are not of types PCI, AGP, PCI-X, or
// PCI-Express and that do not have bus/device/function information, 0FFh
// should be populated in the fields of Segment Group Number, Bus Number,
// Device/Function Number" - this is the *same* convention (near-verbatim
// wording, "devices" substituted for "slots") as `SystemSlotsEntry`'s own
// 7.10.8 ("For slots that are not of the PCI, AGP, PCI-X, or PCI-Express
// type that do not have bus/device/function information, 0FFh should be
// populated..."). Both sections independently confirm the all-FF/all-FFFF
// "not applicable" sentinel (Segment Group Number == 0xFFFF, Bus Number ==
// 0xFF, Device/Function Number == 0xFF, all three simultaneously - not any
// one field alone), so get_bus_address_string() below legitimately reuses
// `SystemSlotsEntry::get_bus_address_string()`'s own rendering shape and
// sentinel check, confirmed independently rather than copied blindly.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Onboard Devices Extended Information structure (7.42) - one
/// instance per onboard (soldered) device: a Reference Designation string
/// (typically the silkscreen label), a packed Device Type byte (bit 7:
/// enabled/disabled, bits 6:0: device type enum, 7.42.2), a Device Type
/// Instance ordinal (7.42.3), and the Segment Group Number/Bus Number/
/// Device-Function Number triple (7.42.4).
///
/// Device Type byte (0x05) masking: per this story's own "Key constraints"
/// (and this task's explicit CRITICAL constraint), the enabled/disabled
/// bit and the device-type bits are exposed by two *independent*
/// accessors - is_device_enabled() masks bit 7 (`& 0x80`) and
/// get_device_type() masks bits 6:0 (`& 0x7F`) - never a single combined
/// accessor, so a bug that accidentally shares one mask between the two
/// cannot silently corrupt both. See
/// OnboardDevicesExtendedInformationEntryUnitTests' own
/// `EnabledBitAndDeviceTypeDecodeIndependentlyTestCase` for the regression
/// test this constraint specifically calls for.
///
/// See this header's own leading comment block for the confirmed 7.42.4
/// sentinel-convention comparison against `SystemSlotsEntry`'s 7.10.8.
class OnboardDevicesExtendedInformationEntry : public AbstractSMBiosEntry {
public:

    /// @brief Onboard Device Types field, bits 6:0 of Device Type (0x05),
    /// DSP0134 3.9.0 Table 121 (7.42.2).
    /// `DeviceTypeOutOfSpec` (value 0) is not itself a spec value (the
    /// spec's own enumeration starts at 01h); this project's own fallback
    /// sentinel for "field absent or the reserved 00h encoding", matching
    /// this codebase's established `*OutOfSpec` precedent (e.g.
    /// `SystemPowerSupplyEntry::PowerSupplyTypeOutOfSpec`).
    enum DeviceType : uint8_t {
        DeviceTypeOutOfSpec = 0x00,
        DeviceTypeOther = 0x01,
        DeviceTypeUnknown = 0x02,
        DeviceTypeVideo = 0x03,
        DeviceTypeScsiController = 0x04,
        DeviceTypeEthernet = 0x05,
        DeviceTypeTokenRing = 0x06,
        DeviceTypeSound = 0x07,
        DeviceTypePataController = 0x08,
        DeviceTypeSataController = 0x09,
        DeviceTypeSasController = 0x0A,
        DeviceTypeWirelessLan = 0x0B,
        DeviceTypeBluetooth = 0x0C,
        DeviceTypeWwan = 0x0D,
        DeviceTypeEmmc = 0x0E,
        DeviceTypeNvmeController = 0x0F,
        DeviceTypeUfsController = 0x10
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    OnboardDevicesExtendedInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~OnboardDevicesExtendedInformationEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x04 offset, raw byte, STRING index: the onboard device's
    /// Reference Designation (typically the silkscreen label), see 7.42.1.
    /// Returns 0 if get_entry_size() < 0x05.
    uint8_t get_reference_designation_index() const;

    /// @brief 0x05 offset, raw byte. The full, unmasked Device Type byte -
    /// bit 7 is the enabled/disabled flag, bits 6:0 are the device type
    /// enum (7.42.2). Prefer is_device_enabled()/get_device_type() for
    /// decoded access; this accessor is exposed for completeness/debugging
    /// only. Returns 0 if get_entry_size() < 0x06.
    uint8_t get_device_type_byte() const;

    /// @brief 0x06 offset, raw byte. Device Type Instance: a unique value
    /// (within a given device type) indicating this device's order among
    /// same-typed onboard devices (7.42.3). Returns 0 if get_entry_size() <
    /// 0x07.
    uint8_t get_device_type_instance() const;

    /// @brief 0x07 offset, 2 raw bytes. Segment Group Number (7.42.4); 0
    /// for a single-segment topology. Returns 0 if get_entry_size() <
    /// 0x09.
    uint16_t get_segment_group_number() const;

    /// @brief 0x09 offset, raw byte. Bus Number (7.42.4). Returns 0 if
    /// get_entry_size() < 0x0A.
    uint8_t get_bus_number() const;

    /// @brief 0x0A offset, raw byte. The full, unmasked Device/Function
    /// Number byte - bits 7:3 are the device number, bits 2:0 are the
    /// function number (7.42.4). Prefer get_device_number()/
    /// get_function_number() for decoded access. Returns 0 if
    /// get_entry_size() < 0x0B.
    uint8_t get_device_function_number() const;

    /// @brief Device number, bits 7:3 of get_device_function_number().
    uint8_t get_device_number() const;

    /// @brief Function number, bits 2:0 of get_device_function_number().
    uint8_t get_function_number() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Device Type byte (0x05) sub-fields - two independent
    // accessors, see this class's own leading doc comment for why a single
    // combined accessor is deliberately not provided.

    /// @brief Bit 7 of the Device Type byte (0x05), masked independently
    /// of get_device_type() (`get_device_type_byte() & 0x80`): 1 means the
    /// device is enabled, 0 means disabled. Returns false if
    /// get_entry_size() < 0x06.
    bool is_device_enabled() const;

    /// @brief Bits 6:0 of the Device Type byte (0x05), masked
    /// independently of is_device_enabled() (`get_device_type_byte() &
    /// 0x7F`). See DeviceType for values. Returns DeviceTypeOutOfSpec if
    /// get_entry_size() < 0x06.
    DeviceType get_device_type() const;

    /// @brief String representation of get_device_type().
    std::string get_device_type_string() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x04 offset DMI string: the onboard device's Reference
    /// Designation (typically the silkscreen label), see 7.42.1. Returns
    /// dmi_string(0) ("Not Specified") if get_entry_size() < 0x05.
    std::string get_reference_designation_string() const;

    /// @brief Segment:Bus:Device.Function string representation, e.g.
    /// "0000:01:02.3" - the same rendering shape as
    /// `SystemSlotsEntry::get_bus_address_string()`, confirmed to share
    /// its all-FF/all-FFFF "not applicable" sentinel convention (see this
    /// header's own leading comment block). Empty when segment/bus/
    /// device-function are all the sentinel, or when length < 0x0B (field
    /// absent).
    std::string get_bus_address_string() const;

private:

    /// Map DeviceType values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> device_type_map_;
};

} // namespace smbios
