#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.hpp>

// Portable Battery entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Portable Battery' chapter
// (7.23), DSP0134 3.9.0. Field offsets, the Table 90 byte layout, and the
// Table 91 (Device Chemistry) enumeration were confirmed directly against
// docs/standard/DSP0134_3.9.0.md's own 7.23/7.23.1 (not assumed from any
// task-document reproduction).
//
// Design Capacity Multiplier resolution (DSP0134 3.9.0 Table 90): the
// multiplier IS a separate structure field - "Design Capacity Multiplier"
// at offset 0x15h, BYTE, 2.2+ - NOT merely descriptive prose about external
// interpretation. Table 90's own text for the 0x0Ah "Design Capacity"
// field reads: "For version 2.2+ implementations, this value is multiplied
// by the Design Capacity Multiplier to produce the actual value," and the
// 0x15h row spells out the field itself ("Multiplication factor of the
// Design Capacity value... The multiplier default is 1, SBDS
// implementations use the value 10..."). Accordingly, get_design_capacity()
// here returns the EFFECTIVE value (raw 0x0A WORD multiplied by the 0x15
// BYTE when that field is present, i.e. get_entry_size() >= 0x16; the
// multiplier defaults to 1 - both when the field is absent on a pre-2.2/
// shorter structure, and defensively if the field is present but reads 0,
// since the spec states the default is 1 and a literal 0 would otherwise
// silently zero out an otherwise-known capacity). The 0x0A field's own
// "0 means unknown" sentinel is checked against the RAW (pre-multiplication)
// value, per is_design_capacity_known()'s own doc comment - this class does
// not expose a separate public accessor for the raw multiplier byte itself,
// since the task's accessor list calls only for the already-resolved
// effective capacity.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Portable Battery structure (7.23) - battery location/
/// manufacturer/device-name/SBDS-version strings, the manufacture date and
/// serial number strings (each with a documented "0 means see the SBDS
/// equivalent instead" 2.2+ Smart Battery convention - this class does NOT
/// decode SBDS data itself, only reports whether the index is 0 via
/// uses_sbds_manufacture_date()/uses_sbds_serial_number()), the device
/// chemistry enum (7.23.1, with 0x02/Unknown being the documented
/// convention for Smart Battery implementations), design capacity (in mWh,
/// see the class-level Design Capacity Multiplier resolution note above)
/// and design voltage (in mV), and the maximum error percentage reported by
/// the battery.
class PortableBatteryEntry : public AbstractSMBiosEntry {
public:

    /// @brief Device Chemistry field (offset 0x09), see 7.23.1's Table 91.
    /// `ChemistryOutOfSpec` (0x00) is not a spec value; it is this
    /// project's own fallback sentinel for "field absent or value not in
    /// Table 91", matching BaseboardInformationEntry::BoardType's own
    /// `BoardTypeOutOfSpec` precedent. `Unknown` (0x02) is also the
    /// documented convention 2.2+ Smart Battery implementations set this
    /// field to, to indicate that the SBDS Device Chemistry field (0x14,
    /// not decoded by this class) carries the real value instead.
    enum DeviceChemistry : uint8_t {
        ChemistryOutOfSpec = 0x00,
        Other = 0x01,
        Unknown = 0x02,
        LeadAcid = 0x03,
        NickelCadmium = 0x04,
        NickelMetalHydride = 0x05,
        LithiumIon = 0x06,
        ZincAir = 0x07,
        LithiumPolymer = 0x08
    };

    /// @brief Maximum Error in Battery Data sentinel (0xFF): the value is
    /// unknown.
    static constexpr uint8_t MaximumErrorUnknown = 0xFF;

    /// @brief Parse the header, recognize how much information do we have
    /// in Portable Battery SMBIOS entry depending on header.length.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    PortableBatteryEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~PortableBatteryEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word values

    /// @brief 0x09 offset. See DeviceChemistry enum for values.
    uint8_t get_device_chemistry() const;

    /// @brief 0x0A offset, 2 raw bytes, EFFECTIVE value: see the class doc
    /// comment's Design Capacity Multiplier resolution note. Returns 0 when
    /// the raw (pre-multiplication) field reads 0 (unknown, per spec) or
    /// when get_entry_size() < 0x0C.
    uint32_t get_design_capacity() const;

    /// @brief 0x0C offset, 2 bytes. Design voltage of the battery in mV.
    /// 0 means unknown, per spec. Returns 0 if get_entry_size() < 0x0E.
    uint16_t get_design_voltage() const;

    /// @brief 0x0F offset. Maximum error (0-100, percentage) in the
    /// Watt-hour data reported by the battery, raw byte passed through
    /// unmodified. 0xFF (MaximumErrorUnknown) means unknown, per spec.
    /// Returns MaximumErrorUnknown if get_entry_size() < 0x10.
    uint8_t get_maximum_error_percent() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether the raw (pre-multiplication) Design Capacity field at
    /// 0x0A is known, i.e. non-zero. See get_design_capacity()'s own doc
    /// comment for why the sentinel check is against the raw field, not the
    /// multiplied effective value. Returns false if get_entry_size() < 0x0C.
    bool is_design_capacity_known() const;

    /// @brief Whether the Design Voltage field at 0x0C is known, i.e.
    /// non-zero. Returns false if get_entry_size() < 0x0E.
    bool is_design_voltage_known() const;

    /// @brief Whether the Maximum Error in Battery Data field at 0x0F is
    /// known, i.e. not MaximumErrorUnknown (0xFF). Returns false if
    /// get_entry_size() < 0x10.
    bool is_maximum_error_known() const;

    /// @brief Whether Manufacture Date Index (0x06) reads 0, the documented
    /// 2.2+ Smart Battery convention meaning "see the SBDS Manufacture Date
    /// field instead" (not decoded by this class). Also true when the field
    /// is physically absent (get_entry_size() < 0x07), since the index then
    /// defaults to 0 as well.
    bool uses_sbds_manufacture_date() const;

    /// @brief Whether Serial Number Index (0x07) reads 0, the documented
    /// 2.2+ Smart Battery convention meaning "see the SBDS Serial Number
    /// field instead" (not decoded by this class). Also true when the field
    /// is physically absent (get_entry_size() < 0x08).
    bool uses_sbds_serial_number() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Location DMI string (0x04); example per spec: "in the back,
    /// on the left-hand side".
    std::string get_location_string() const;

    /// @brief Manufacturer DMI string (0x05).
    std::string get_manufacturer_string() const;

    /// @brief Manufacture Date DMI string (0x06). Empty when
    /// uses_sbds_manufacture_date() is true (index 0, "see SBDS Manufacture
    /// Date instead") - this class does not decode SBDS data.
    std::string get_manufacture_date_string() const;

    /// @brief Serial Number DMI string (0x07). Empty when
    /// uses_sbds_serial_number() is true (index 0, "see SBDS Serial Number
    /// instead") - this class does not decode SBDS data.
    std::string get_serial_number_string() const;

    /// @brief Device Name DMI string (0x08); example per spec: "DR-36".
    std::string get_device_name_string() const;

    /// @brief Device Chemistry string representation; see
    /// get_device_chemistry().
    std::string get_device_chemistry_string() const;

    /// @brief SBDS Version Number DMI string (0x0E); empty if the battery
    /// does not support the Smart Battery Data Specification function, per
    /// spec.
    std::string get_sbds_version_string() const;

private:

    /// Map flags/codes data to string values
    void init_string_values();

    /// @brief 0x15 offset, 2.2+. Multiplication factor for the raw 0x0A
    /// Design Capacity field; see the class doc comment's resolution note.
    /// Returns 1 (the spec's own documented default) when the field is
    /// absent (get_entry_size() < 0x16) or reads 0.
    uint8_t design_capacity_multiplier() const;

    /// @brief Raw (pre-multiplication) 0x0A Design Capacity WORD. Returns 0
    /// if get_entry_size() < 0x0C.
    uint16_t raw_design_capacity() const;

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> device_chemistry_map_;
};

} // namespace smbios
