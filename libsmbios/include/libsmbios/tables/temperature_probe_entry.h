#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>

// Temperature Probe entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Temperature Probe'
// chapter (7.29), DSP0134 3.9.0. Field offsets (Table 99) and the Location
// and Status bit layout (7.29.1, Table 100) were confirmed directly
// against docs/standard/DSP0134_3.9.0.md, not assumed from the task
// document's own field list, and not assumed to be byte-identical to
// VoltageProbeEntry's Table 96/VoltageProbeEntry despite the near-identical
// shape - see the cross-check note below.
//
// Deviation from this milestone's own roadmap task document (Story 04.0,
// Task 03): same finding as VoltageProbeEntry's header - the task document
// omits the 0x14 Nominal Value field for this type, but Table 99 as it
// actually reads in docs/standard/DSP0134_3.9.0.md shows it present
// (1/10th degree C, `0x8000` = unknown, present only if the structure's
// length allows it), identically shaped to Voltage/Electrical Current
// Probe's own 0x14 field. This class implements the field actually present
// in the standard.
//
// Status/Location bit-layout cross-check (per this task's own explicit
// instruction to confirm 7.27.1/7.29.1/7.30.1 independently): the Status
// sub-field (bits 7:5) is confirmed BYTE-IDENTICAL to VoltageProbeEntry's
// and ElectricalCurrentProbeEntry's own Status (same six values, same bit
// positions - Table 96/Table 100/Table 102 all agree). The Location
// sub-field (bits 4:0) is NOT identical: Table 100 is a 15-value SUPERSET
// of Table 96/102's 11-value list, adding Front Panel Board/Back Panel
// Board/Power System Board/Drive Back Plane (values 12-15) - this class's
// own Location enum reflects that superset, unlike VoltageProbeEntry's/
// ElectricalCurrentProbeEntry's 11-value Location.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Temperature Probe structure (7.29) - a single temperature
/// probe's physical location/status, its description string, its readable
/// maximum/minimum/nominal values, resolution, tolerance and accuracy (all
/// in units of degrees Celsius at various sub-degree resolutions per field,
/// each independently `0x8000`-sentineled "unknown"), and an OEM-defined
/// DWORD.
class TemperatureProbeEntry : public AbstractSMBiosEntry {
public:

    /// @brief Status sub-field (bits 7:5 of Location and Status), DSP0134
    /// 3.9.0 Table 100 - confirmed byte-identical to VoltageProbeEntry's/
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

    /// @brief Location sub-field (bits 4:0 of Location and Status), DSP0134
    /// 3.9.0 Table 100 - a 15-value SUPERSET of VoltageProbeEntry's/
    /// ElectricalCurrentProbeEntry's own 11-value Location, see class doc
    /// comment. `LocationOutOfSpec` (value 0) is not a spec value; this
    /// project's own fallback sentinel for "field absent or an unassigned
    /// 5-bit value".
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
        LocationAddInCard = 0x0B,
        LocationFrontPanelBoard = 0x0C,
        LocationBackPanelBoard = 0x0D,
        LocationPowerSystemBoard = 0x0E,
        LocationDriveBackPlane = 0x0F
    };

    /// @brief Maximum/Minimum/Resolution/Tolerance/Accuracy/Nominal Value
    /// sentinel (0x8000): the value is unknown, per spec, for every one of
    /// these WORD fields.
    static constexpr uint16_t ValueUnknown = 0x8000;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    TemperatureProbeEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~TemperatureProbeEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword values

    /// @brief 0x05 offset, raw byte. See Status/Location enums and the
    /// class doc comment for the confirmed Table 100 bit layout. Returns 0
    /// if get_entry_size() < 0x06.
    uint8_t get_location_and_status() const;

    /// @brief 0x06 offset, 2 raw bytes, 1/10th degree C. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x08.
    uint16_t get_maximum_value() const;

    /// @brief 0x08 offset, 2 raw bytes, 1/10th degree C. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0A.
    uint16_t get_minimum_value() const;

    /// @brief 0x0A offset, 2 raw bytes, 1/1000th degree C. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0C.
    uint16_t get_resolution() const;

    /// @brief 0x0C offset, 2 raw bytes, +/- 1/10th degree C. ValueUnknown
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

    /// @brief 0x14 offset, 2 raw bytes, 1/10th degree C - see class doc
    /// comment for why this class exposes it despite the roadmap task
    /// document's own (incomplete) field list. ValueUnknown (0x8000) means
    /// unknown, per spec. Present only if has_nominal_value() is true;
    /// returns ValueUnknown otherwise.
    uint16_t get_nominal_value() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Description DMI string (0x04); additional descriptive
    /// information about the probe or its location.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Location and Status sub-fields (Table 100)

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
    /// structure - see VoltageProbeEntry::has_nominal_value()'s doc comment
    /// for why the guard is get_entry_size() >= 0x16 (the full 2-byte
    /// field), not the literal spec text's 0x15.
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
