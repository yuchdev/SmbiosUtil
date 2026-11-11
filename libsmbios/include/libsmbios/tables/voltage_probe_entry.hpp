#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Voltage Probe entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Voltage Probe' chapter
// (7.27), DSP0134 3.9.0. Field offsets (Table 95) and the Location and
// Status bit layout (7.27.1, Table 96) were confirmed directly against
// docs/standard/DSP0134_3.9.0.md, not assumed from the task document's own
// field list.
//
// Deviation from this milestone's own roadmap task document (Story 04.0,
// Task 03): that document's field list for Voltage Probe stops at 0x10
// OEM-defined and only attributes a 0x14 Nominal Value field to Electrical
// Current Probe (29). Table 95 as it actually reads in
// docs/standard/DSP0134_3.9.0.md shows the IDENTICAL 0x14 Nominal Value
// WORD field (mV, `0x8000` = unknown, present only if the structure's
// length allows it) for Voltage Probe too - this class implements the
// field actually present in the standard, not the shorter list in the task
// document. See TemperatureProbeEntry's own header for the identical
// finding on that class.
//
// Status/Location bit-layout cross-check (per this task's own explicit
// instruction to confirm 7.27.1/7.29.1/7.30.1 independently rather than
// assume a shared table): the Status sub-field (bits 7:5) is confirmed
// BYTE-IDENTICAL across all three probe types' own numbered subsections
// (Table 96/Table 100/Table 102 all enumerate the same six values at the
// same bit positions: Other/Unknown/OK/Non-critical/Critical/
// Non-recoverable). The Location sub-field (bits 4:0) is NOT identical
// across all three: Voltage Probe (Table 96) and Electrical Current Probe
// (Table 102) share the identical 11-value list (Other..Add-in Card, values
// 1-11), but Temperature Probe (Table 100) defines a 15-value superset
// (the same 11 plus Front Panel Board/Back Panel Board/Power System
// Board/Drive Back Plane, values 12-15). This class's own Location enum
// matches Table 96/102's 11-value list, matching the codebase's existing
// per-class nested-enum convention (see SystemResetEntry::BootOption)
// rather than introducing a new shared cross-class enum type.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Voltage Probe structure (7.27) - a single voltage probe's
/// physical location/status, its description string, its readable
/// maximum/minimum/nominal values, resolution, tolerance and accuracy (all
/// in millivolts or sub-millivolt units per field, each independently
/// `0x8000`-sentineled "unknown"), and an OEM-defined DWORD.
class VoltageProbeEntry : public AbstractSMBiosEntry {
public:

    /// @brief Status sub-field (bits 7:5 of Location and Status), DSP0134
    /// 3.9.0 Table 96 - confirmed byte-identical to Table 100 (Temperature
    /// Probe) and Table 102 (Electrical Current Probe), see class doc
    /// comment. `StatusOutOfSpec` (value 0, binary 000) is not a spec
    /// value; it is this project's own fallback sentinel for "field absent
    /// or reserved 3-bit value", matching this codebase's established
    /// `*OutOfSpec` precedent (e.g. BaseboardInformationEntry::BoardType).
    /// Binary 111 is likewise reserved and decodes to this same sentinel.
    enum Status : uint8_t {
        StatusOutOfSpec = 0x00,
        StatusOther = 0x01,
        StatusUnknown = 0x02,
        StatusOk = 0x03,
        StatusNonCritical = 0x04,
        StatusCritical = 0x05,
        StatusNonRecoverable = 0x06
    };

    /// @brief Location sub-field (bits 4:0 of Location and Status), DSP0134
    /// 3.9.0 Table 96 - confirmed byte-identical to Table 102 (Electrical
    /// Current Probe) but NOT to Table 100 (Temperature Probe, which adds
    /// four more values), see class doc comment. `LocationOutOfSpec`
    /// (value 0) is not a spec value; this project's own fallback sentinel
    /// for "field absent or an unassigned 5-bit value".
    enum Location : uint8_t {
        LocationOutOfSpec = 0x00,
        LocationOther = 0x01,
        LocationUnknown = 0x02,
        LocationProcessor = 0x03,
        LocationDisk = 0x04,
        LocationPeripheralBay = 0x05,
        LocationSystemManagementModule = 0x06,
        LocationMotherboard = 0x07,
        LocationMemoryModule = 0x08,
        LocationProcessorModule = 0x09,
        LocationPowerUnit = 0x0A,
        LocationAddInCard = 0x0B
    };

    /// @brief Maximum/Minimum/Resolution/Tolerance/Accuracy/Nominal Value
    /// sentinel (0x8000): the value is unknown, per spec, for every one of
    /// these WORD fields.
    static constexpr uint16_t ValueUnknown = 0x8000;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    VoltageProbeEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~VoltageProbeEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order. Every
    /// WORD reading stays the printed string - "<value><unit>" or the
    /// "Unknown" sentinel (0x8000) - and Nominal Value appears exactly when
    /// the text prints it (has_nominal_value()). OEM-defined is not printed,
    /// so it is not a field.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword values

    /// @brief 0x05 offset, raw byte. See Status/Location enums and the
    /// class doc comment for the confirmed Table 96 bit layout. Returns 0
    /// if get_entry_size() < 0x06.
    uint8_t get_location_and_status() const;

    /// @brief 0x06 offset, 2 raw bytes, millivolts. ValueUnknown (0x8000)
    /// means unknown, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x08.
    uint16_t get_maximum_value() const;

    /// @brief 0x08 offset, 2 raw bytes, millivolts. ValueUnknown (0x8000)
    /// means unknown, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x0A.
    uint16_t get_minimum_value() const;

    /// @brief 0x0A offset, 2 raw bytes, tenths of millivolts. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0C.
    uint16_t get_resolution() const;

    /// @brief 0x0C offset, 2 raw bytes, +/- millivolts. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0E.
    uint16_t get_tolerance() const;

    /// @brief 0x0E offset, 2 raw bytes, +/- 1/100th of a percent.
    /// ValueUnknown (0x8000) means unknown, per spec. Returns ValueUnknown
    /// if get_entry_size() < 0x10.
    uint16_t get_accuracy() const;

    /// @brief 0x10 offset, 4 raw bytes. OEM- or firmware vendor-specific
    /// information. Returns 0 if get_entry_size() < 0x14.
    uint32_t get_oem_defined() const;

    /// @brief 0x14 offset, 2 raw bytes, millivolts - see class doc comment
    /// for why this class exposes it despite the roadmap task document's
    /// own (incomplete) field list. ValueUnknown (0x8000) means unknown,
    /// per spec. Present only if has_nominal_value() is true; returns
    /// ValueUnknown otherwise.
    uint16_t get_nominal_value() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Description DMI string (0x04); additional descriptive
    /// information about the probe or its location.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Location and Status sub-fields (Table 96)

    /// @brief Bits 7:5 of Location and Status. See Status for values.
    /// Returns StatusOutOfSpec if get_entry_size() < 0x06.
    Status get_status() const;

    /// @brief String representation of get_status().
    std::string get_status_string() const;

    /// @brief Bits 4:0 of Location and Status. See Location for values.
    /// Returns LocationOutOfSpec if get_entry_size() < 0x06.
    Location get_location() const;

    /// @brief String representation of get_location().
    std::string get_location_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Maximum Value (0x06) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x08.
    bool is_maximum_value_known() const;

    /// @brief Whether Minimum Value (0x08) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0A.
    bool is_minimum_value_known() const;

    /// @brief Whether Resolution (0x0A) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0C.
    bool is_resolution_known() const;

    /// @brief Whether Tolerance (0x0C) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0E.
    bool is_tolerance_known() const;

    /// @brief Whether Accuracy (0x0E) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x10.
    bool is_accuracy_known() const;

    /// @brief Whether Nominal Value (0x14) is physically present in this
    /// structure. Per spec text ("present ... only if the structure's
    /// length is larger than 14h") the gate is length > 0x14, but since
    /// Nominal Value is a 2-byte WORD, a length of exactly 0x15 could only
    /// supply its low byte - this accessor therefore requires the full
    /// field (get_entry_size() >= 0x16) before reporting it present,
    /// treating a hypothetical 0x15-length structure defensively as "not
    /// yet present" rather than reading one byte of the adjacent string
    /// section as if it were Nominal Value's high byte.
    bool has_nominal_value() const;

    /// @brief Whether Nominal Value (0x14) is known, i.e. physically
    /// present (has_nominal_value()) and not ValueUnknown.
    bool is_nominal_value_known() const;

private:

    /// Map Status/Location values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> status_map_;
    std::map<uint8_t, std::string> location_map_;
};

} // namespace smbios
