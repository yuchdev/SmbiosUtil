#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Cooling Device entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Cooling Device' chapter
// (7.28), DSP0134 3.9.0. Field offsets (Table 97) and the Device Type and
// Status bit layout (7.28.1, Table 98) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md.
//
// Status cross-check: the Status sub-field (bits 7:5 of Device Type and
// Status, Table 98) is confirmed byte-identical to VoltageProbeEntry's/
// TemperatureProbeEntry's/ElectricalCurrentProbeEntry's own Status
// sub-field (same six values, same bit positions). Device Type (bits 4:0)
// is this structure's own enumeration (fans/blowers/heat pipes/
// refrigeration/active-passive cooling), unrelated to any probe's
// Location enumeration.
//
// Trailing-field length gates (Table 97): Nominal Speed (0x0C, 2.2+) and
// Description (0x0E, 2.7+) are each independently length-gated. Nominal
// Speed's spec text reads "present ... only if the structure's length is
// larger than 0Ch" (i.e. length > 0x0C) - but since Nominal Speed is a
// 2-byte WORD, a length of exactly 0x0D could only supply its low byte;
// this class therefore requires the FULL field (length >= 0x0E) before
// reporting it present - see has_nominal_speed()'s own doc comment.
// Description's gate ("0Fh or larger") has no such off-by-one concern
// since it is a single-byte STRING index, so has_description() uses the
// literal length >= 0x0F guard.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Cooling Device structure (7.28) - a cooling device's type/
/// status, the Temperature Probe structure monitoring it, its cooling-unit
/// redundancy group, an OEM-defined DWORD, and two independently
/// length-gated trailing fields: nominal rotational speed (2.2+) and a
/// description string (2.7+).
class CoolingDeviceEntry : public AbstractSMBiosEntry {
public:

    /// @brief Status sub-field (bits 7:5 of Device Type and Status),
    /// DSP0134 3.9.0 Table 98 - confirmed byte-identical to
    /// VoltageProbeEntry's/TemperatureProbeEntry's/
    /// ElectricalCurrentProbeEntry's own Status, see class doc comment.
    /// `StatusOutOfSpec` (value 0, binary 000) is not a spec value; this
    /// project's own fallback sentinel for "field absent or reserved 3-bit
    /// value" (binary 111 is likewise reserved and decodes to this same
    /// sentinel).
    enum Status : uint8_t {
        StatusOutOfSpec = 0x00,
        StatusOther = 0x01,
        StatusUnknown = 0x02,
        StatusOk = 0x03,
        StatusNonCritical = 0x04,
        StatusCritical = 0x05,
        StatusNonRecoverable = 0x06
    };

    /// @brief Device Type sub-field (bits 4:0 of Device Type and Status),
    /// DSP0134 3.9.0 Table 98. `DeviceTypeOutOfSpec` (value 0) is not a
    /// spec value; this project's own fallback sentinel for "field absent
    /// or an unassigned 5-bit value".
    enum DeviceType : uint8_t {
        DeviceTypeOutOfSpec = 0x00,
        DeviceTypeOther = 0x01,
        DeviceTypeUnknown = 0x02,
        DeviceTypeFan = 0x03,
        DeviceTypeCentrifugalBlower = 0x04,
        DeviceTypeChipFan = 0x05,
        DeviceTypeCabinetFan = 0x06,
        DeviceTypePowerSupplyFan = 0x07,
        DeviceTypeHeatPipe = 0x08,
        DeviceTypeIntegratedRefrigeration = 0x09,
        DeviceTypeActiveCooling = 0x10,
        DeviceTypePassiveCooling = 0x11
    };

    /// @brief Nominal Speed sentinel (0x8000): the value is unknown or the
    /// cooling device is non-rotating, per spec.
    static constexpr uint16_t NominalSpeedUnknown = 0x8000;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    CoolingDeviceEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~CoolingDeviceEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword values

    /// @brief 0x04 offset, 2 raw bytes. Handle, or instance number, of the
    /// temperature probe (7.29) monitoring this cooling device. Returns
    /// 0xFFFF if get_entry_size() < 0x06.
    uint16_t get_temperature_probe_handle() const;

    /// @brief 0x06 offset, raw byte. See Status/DeviceType enums and the
    /// class doc comment for the confirmed Table 98 bit layout. Returns 0
    /// if get_entry_size() < 0x07.
    uint8_t get_device_type_and_status() const;

    /// @brief 0x07 offset, raw byte. Cooling unit group this device is
    /// associated with; 0 means the device is not a member of a redundant
    /// cooling unit (see is_redundant()). Returns 0 if get_entry_size() <
    /// 0x08.
    uint8_t get_cooling_unit_group() const;

    /// @brief 0x08 offset, 4 raw bytes. OEM- or firmware vendor-specific
    /// information. Returns 0 if get_entry_size() < 0x0C.
    uint32_t get_oem_defined() const;

    /// @brief 0x0C offset, 2 raw bytes, rpm. NominalSpeedUnknown (0x8000)
    /// means unknown or non-rotating, per spec. Present only if
    /// has_nominal_speed() is true; returns NominalSpeedUnknown otherwise.
    uint16_t get_nominal_speed() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief 0x0E offset DMI string; additional descriptive information
    /// about the cooling device or its location. Present only if
    /// has_description() is true; returns an empty string otherwise.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Device Type and Status sub-fields (Table 98)

    /// @brief Bits 7:5 of Device Type and Status. See Status for values.
    /// Returns StatusOutOfSpec if get_entry_size() < 0x07.
    Status get_status() const;

    /// @brief String representation of get_status().
    std::string get_status_string() const;

    /// @brief Bits 4:0 of Device Type and Status. See DeviceType for
    /// values. Returns DeviceTypeOutOfSpec if get_entry_size() < 0x07.
    DeviceType get_device_type() const;

    /// @brief String representation of get_device_type().
    std::string get_device_type_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Cooling Unit Group (0x07) is non-zero, i.e. this
    /// device shares its group with at least one other cooling device
    /// (redundant configuration), per spec. Returns false if
    /// get_entry_size() < 0x08.
    bool is_redundant() const;

    /// @brief Whether Nominal Speed (0x0C) is physically present in this
    /// structure. Per spec text ("present ... only if the structure's
    /// length is larger than 0Ch") the gate is length > 0x0C, but since
    /// Nominal Speed is a 2-byte WORD, a length of exactly 0x0D could only
    /// supply its low byte - this accessor therefore requires the full
    /// field (get_entry_size() >= 0x0E) before reporting it present,
    /// treating a hypothetical 0x0D-length structure defensively as "not
    /// yet present" rather than reading one byte of the adjacent
    /// Description/string section as if it were Nominal Speed's high byte.
    bool has_nominal_speed() const;

    /// @brief Whether Nominal Speed (0x0C) is known, i.e. physically
    /// present (has_nominal_speed()) and not NominalSpeedUnknown.
    bool is_nominal_speed_known() const;

    /// @brief Whether Description (0x0E) is physically present in this
    /// structure, per spec text ("present ... only if the structure's
    /// length is 0Fh or larger"), i.e. get_entry_size() >= 0x0F. Unlike
    /// has_nominal_speed(), no off-by-one adjustment is needed here since
    /// Description is a single-byte STRING index.
    bool has_description() const;

private:

    /// Map Status/DeviceType values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> status_map_;
    std::map<uint8_t, std::string> device_type_map_;
};

} // namespace smbios
